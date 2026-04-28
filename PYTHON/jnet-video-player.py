#!/usr/bin/env python3
"""
J~NET Video Player — PyQt6 / Wayland version
Raspberry Pi 5 arm64 optimized
Uses python-mpv as playback backend with PyQt6 GUI
"""

import sys
import os
import json
import subprocess
import glob
from pathlib import Path

from PyQt6.QtCore import (
    Qt, QTimer, QUrl, QSize, QRect, QRectF, QPropertyAnimation,
    pyqtSignal, pyqtSlot, QEvent, QPoint, pyqtProperty
)
from PyQt6.QtGui import (
    QAction, QColor, QPalette, QFont, QIcon, QPainter,
    QBrush, QPen, QPixmap, QLinearGradient, QCursor, QFontDatabase,
    QKeySequence, QShortcut, QMouseEvent
)
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QSlider, QLabel, QFileDialog, QListWidget,
    QListWidgetItem, QSplitter, QFrame, QScrollArea, QAbstractSlider,
    QSizePolicy, QToolTip, QStyle, QStyleFactory
)
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
from PyQt6.QtMultimediaWidgets import QVideoWidget

try:
    import mpv
    HAS_MPV = True
except (ImportError, OSError):
    HAS_MPV = False

HAS_MPV_SO = HAS_MPV
# Also try loading via ctypes directly
if not HAS_MPV:
    import ctypes.util
    lib_path = ctypes.util.find_library('mpv')
    if lib_path is None:
        # Check common locations
        for p in ['/usr/lib/aarch64-linux-gnu/libmpv.so', '/usr/lib/libmpv.so', '/usr/local/lib/libmpv.so']:
            if os.path.exists(p):
                lib_path = p
                break


APP_NAME = "J~NET Video Player"
APP_VERSION = "1.0.0"
DARK_PALETTE = {
    "bg": "#0d0d0d",
    "bg2": "#141414",
    "bg3": "#1a1a1a",
    "surface": "#1e1e1e",
    "surface2": "#252525",
    "border": "#333333",
    "accent": "#339933",
    "accent_hover": "#40b340",
    "text": "#e0e0e0",
    "text_dim": "#808080",
    "text_bright": "#ffffff",
    "danger": "#cc3333",
    "warning": "#cc9933",
}

STYLESHEET = f"""
QMainWindow, QWidget {{
    background-color: {DARK_PALETTE['bg']};
    color: {DARK_PALETTE['text']};
    font-family: 'Cantarell', 'Noto Sans', 'Ubuntu', sans-serif;
    font-size: 13px;
}}
QPushButton {{
    background-color: {DARK_PALETTE['surface']};
    color: {DARK_PALETTE['text']};
    border: 1px solid {DARK_PALETTE['border']};
    border-radius: 4px;
    padding: 6px 14px;
    min-height: 28px;
}}
QPushButton:hover {{
    background-color: {DARK_PALETTE['surface2']};
    border-color: {DARK_PALETTE['accent']};
}}
QPushButton:pressed {{
    background-color: {DARK_PALETTE['accent']};
    color: white;
}}
QPushButton#playBtn {{
    background-color: {DARK_PALETTE['accent']};
    color: white;
    font-size: 16px;
    min-width: 50px;
    border: none;
    border-radius: 20px;
    padding: 8px 20px;
}}
QPushButton#playBtn:hover {{
    background-color: {DARK_PALETTE['accent_hover']};
}}
QSlider::groove:horizontal {{
    background: {DARK_PALETTE['border']};
    height: 4px;
    border-radius: 2px;
}}
QSlider::handle:horizontal {{
    background: {DARK_PALETTE['accent']};
    width: 14px;
    height: 14px;
    margin: -5px 0;
    border-radius: 7px;
}}
QSlider::sub-page:horizontal {{
    background: {DARK_PALETTE['accent']};
    border-radius: 2px;
}}
QSlider::groove:vertical {{
    background: {DARK_PALETTE['border']};
    width: 4px;
    border-radius: 2px;
}}
QSlider::handle:vertical {{
    background: {DARK_PALETTE['accent']};
    width: 14px;
    height: 14px;
    margin: 0 -5px;
    border-radius: 7px;
}}
QSlider::sub-page:vertical {{
    background: {DARK_PALETTE['accent']};
    border-radius: 2px;
}}
QListWidget {{
    background-color: {DARK_PALETTE['bg2']};
    border: 1px solid {DARK_PALETTE['border']};
    border-radius: 4px;
    outline: none;
    padding: 2px;
}}
QListWidget::item {{
    padding: 8px 12px;
    border-radius: 3px;
    margin: 1px 0;
}}
QListWidget::item:selected {{
    background-color: {DARK_PALETTE['accent']};
    color: white;
}}
QListWidget::item:hover {{
    background-color: {DARK_PALETTE['surface2']};
}}
QLabel {{
    background: transparent;
    color: {DARK_PALETTE['text']};
}}
QScrollBar:vertical {{
    background: {DARK_PALETTE['bg2']};
    width: 8px;
    border-radius: 4px;
}}
QScrollBar::handle:vertical {{
    background: {DARK_PALETTE['border']};
    min-height: 30px;
    border-radius: 4px;
}}
QScrollBar::handle:vertical:hover {{
    background: {DARK_PALETTE['accent']};
}}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}
QFrame#controls {{
    background-color: rgba(13, 13, 13, 200);
    border-top: 1px solid {DARK_PALETTE['border']};
}}
QFrame#controls QPushButton {{
    background: transparent;
    border: none;
    color: {DARK_PALETTE['text']};
    padding: 6px 10px;
    font-size: 14px;
}}
QFrame#controls QPushButton:hover {{
    color: {DARK_PALETTE['accent']};
}}
QFrame#titlebar {{
    background-color: rgba(13, 13, 13, 200);
    border-bottom: 1px solid {DARK_PALETTE['border']};
}}
"""


class VideoControls(QWidget):
    """Floating video controls overlay"""

    playToggled = pyqtSignal()
    stopped = pyqtSignal()
    seekRequested = pyqtSignal(float)
    volumeChanged = pyqtSignal(float)
    fullscreenToggled = pyqtSignal()
    fileOpened = pyqtSignal()
    playlistToggled = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("controls")
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self._opacity = 0.0
        self._animating = False
        self._setup_ui()
        self._fade_timer = QTimer(self)
        self._fade_timer.setSingleShot(True)
        self._fade_timer.setInterval(3000)
        self._fade_timer.timeout.connect(self.fade_out)

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 4, 12, 8)
        layout.setSpacing(4)

        # Seek bar
        seek_layout = QHBoxLayout()
        seek_layout.setContentsMargins(0, 0, 0, 0)
        seek_layout.setSpacing(8)

        self.time_label = QLabel("00:00 / 00:00")
        self.time_label.setStyleSheet("color: #aaa; font-size: 11px;")
        self.time_label.setFixedWidth(120)

        self.seek_slider = QSlider(Qt.Orientation.Horizontal)
        self.seek_slider.setRange(0, 1000)
        self.seek_slider.setValue(0)
        self.seek_slider.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.seek_slider.sliderMoved.connect(self._on_seek)

        seek_layout.addWidget(self.time_label)
        seek_layout.addWidget(self.seek_slider)

        # Button row
        btn_layout = QHBoxLayout()
        btn_layout.setContentsMargins(0, 0, 0, 0)
        btn_layout.setSpacing(0)

        self.play_btn = QPushButton("▶")
        self.play_btn.setObjectName("playBtn")
        self.play_btn.setFixedSize(44, 36)
        self.play_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.play_btn.clicked.connect(self.playToggled.emit)

        self.stop_btn = QPushButton("■")
        self.stop_btn.setFixedSize(36, 28)
        self.stop_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.stop_btn.clicked.connect(self.stopped.emit)

        btn_layout.addWidget(self.play_btn)
        btn_layout.addSpacing(6)
        btn_layout.addWidget(self.stop_btn)
        btn_layout.addSpacing(16)

        # Volume
        vol_label = QLabel("🔊")
        vol_label.setStyleSheet("font-size: 14px;")
        self.vol_slider = QSlider(Qt.Orientation.Horizontal)
        self.vol_slider.setRange(0, 100)
        self.vol_slider.setValue(80)
        self.vol_slider.setFixedWidth(100)
        self.vol_slider.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.vol_slider.valueChanged.connect(lambda v: self.volumeChanged.emit(v / 100.0))

        btn_layout.addWidget(vol_label)
        btn_layout.addWidget(self.vol_slider)
        btn_layout.addStretch()

        self.open_btn = QPushButton("📂 Open")
        self.open_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.open_btn.clicked.connect(self.fileOpened.emit)
        btn_layout.addWidget(self.open_btn)

        self.pl_btn = QPushButton("📋 Playlist")
        self.pl_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.pl_btn.clicked.connect(self.playlistToggled.emit)
        btn_layout.addWidget(self.pl_btn)

        self.fs_btn = QPushButton("⛶")
        self.fs_btn.setToolTip("Fullscreen")
        self.fs_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        self.fs_btn.clicked.connect(self.fullscreenToggled.emit)
        btn_layout.addWidget(self.fs_btn)

        layout.addLayout(seek_layout)
        layout.addLayout(btn_layout)

    def _on_seek(self, value):
        self.seekRequested.emit(value / 1000.0)

    def set_time(self, current, total):
        def fmt(s):
            s = int(s)
            return f"{s // 60:02d}:{s % 60:02d}"
        self.time_label.setText(f"{fmt(current)} / {fmt(total)}")

    def set_playing(self, playing):
        self.play_btn.setText("⏸" if playing else "▶")

    def fade_out(self):
        if not self.underMouse():
            self.hide()

    def enterEvent(self, event):
        self.show()
        self._fade_timer.stop()

    def leaveEvent(self, event):
        self._fade_timer.start()

    def showEvent(self, event):
        super().showEvent(event)
        self._fade_timer.start()


class PlaylistWidget(QWidget):
    """Sidebar playlist panel"""

    fileSelected = pyqtSignal(str)
    fileRemoved = pyqtSignal(int)
    cleared = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("playlist")
        self.setFixedWidth(280)
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Header
        header = QFrame()
        header.setObjectName("titlebar")
        header.setFixedHeight(40)
        hl = QHBoxLayout(header)
        hl.setContentsMargins(12, 0, 8, 0)
        title = QLabel("📋 Playlist")
        title.setStyleSheet("font-weight: bold; font-size: 13px;")
        hl.addWidget(title)
        hl.addStretch()

        self.count_label = QLabel("0 files")
        self.count_label.setStyleSheet(f"color: {DARK_PALETTE['text_dim']}; font-size: 11px;")
        hl.addWidget(self.count_label)

        clear_btn = QPushButton("✕")
        clear_btn.setFixedSize(24, 24)
        clear_btn.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
        clear_btn.clicked.connect(self._clear)
        hl.addWidget(clear_btn)

        layout.addWidget(header)

        # List
        self.list_widget = QListWidget()
        self.list_widget.setAlternatingRowColors(True)
        self.list_widget.itemDoubleClicked.connect(self._on_item_activated)
        self.list_widget.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.list_widget.customContextMenuRequested.connect(self._context_menu)
        layout.addWidget(self.list_widget)

    def _on_item_activated(self, item):
        path = item.data(Qt.ItemDataRole.UserRole)
        if path:
            self.fileSelected.emit(path)

    def _context_menu(self, pos):
        item = self.list_widget.itemAt(pos)
        if not item:
            return
        from PyQt6.QtWidgets import QMenu
        menu = QMenu(self)
        remove_action = menu.addAction("Remove from playlist")
        action = menu.exec(self.list_widget.mapToGlobal(pos))
        if action == remove_action:
            row = self.list_widget.row(item)
            self.list_widget.takeItem(row)
            self.fileRemoved.emit(row)
            self.count_label.setText(f"{self.list_widget.count()} files")

    def add_file(self, path):
        name = os.path.basename(path)
        item = QListWidgetItem(f"  {name}")
        item.setData(Qt.ItemDataRole.UserRole, path)
        self.list_widget.addItem(item)
        self.count_label.setText(f"{self.list_widget.count()} files")
        return self.list_widget.count() - 1

    def select_index(self, idx):
        if 0 <= idx < self.list_widget.count():
            self.list_widget.setCurrentRow(idx)

    def _clear(self):
        self.list_widget.clear()
        self.count_label.setText("0 files")
        self.cleared.emit()

    def get_paths(self):
        paths = []
        for i in range(self.list_widget.count()):
            item = self.list_widget.item(i)
            p = item.data(Qt.ItemDataRole.UserRole)
            if p:
                paths.append(p)
        return paths


class VideoPlayerWidget(QWidget):
    """The main video display area with overlay controls"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.setStyleSheet(f"background-color: {DARK_PALETTE['bg']};")

        self._video_widget = QVideoWidget(self)
        self._video_widget.setAspectRatioMode(Qt.AspectRatioMode.KeepAspectRatio)
        self._video_widget.setStyleSheet("background: black;")

        self._controls = VideoControls(self)
        self._controls.hide()

    def video_widget(self):
        return self._video_widget

    def controls(self):
        return self._controls

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._video_widget.setGeometry(self.rect())
        cw = self.width()
        ch = 110
        self._controls.setGeometry(0, self.height() - ch, cw, ch)

    def enterEvent(self, event):
        self._controls.show()

    def leaveEvent(self, event):
        if not self._controls.underMouse() and not self._controls._fade_timer.isActive():
            self._controls._fade_timer.start()

    def mouseDoubleClickEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._controls.fullscreenToggled.emit()


class MainWindow(QMainWindow):
    """J~NET Video Player main application window"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle(APP_NAME)
        self.setMinimumSize(800, 500)
        self.resize(1280, 720)

        # State
        self._playlist_paths = []
        self._current_index = -1
        self._is_playing = False
        self._is_fullscreen = False
        self._duration = 0
        self._update_timer = QTimer(self)
        self._update_timer.setInterval(250)
        self._update_timer.timeout.connect(self._update_ui)

        # Setup media player with python-mpv if available
        self._use_mpv = HAS_MPV_SO
        if self._use_mpv:
            self._setup_mpv()
        else:
            self._setup_qtmedia()

        # Build UI
        self._setup_ui()

        # Shortcuts
        self._setup_shortcuts()

        # Apply dark theme
        self._apply_dark_theme()

    def _setup_mpv(self):
        """Use python-mpv for playback"""
        self._mpv_player = mpv.MPV(
            wid=str(int(self._video_widget.winId())),
            input_default_bindings=True,
            input_vo_keyboard=True,
            osc=True,
            volume=80,
            cache=True,
            cache_pause=True,
            demuxer_max_back_bytes=50 * 1024 * 1024,
            demuxer_max_bytes=150 * 1024 * 1024,
        )
        self._mpv_player.observe_property("time-pos", self._mpv_time_changed)
        self._mpv_player.observe_property("duration", self._mpv_duration_changed)
        self._mpv_player.observe_property("pause", self._mpv_pause_changed)
        self._mpv_player.observe_property("eof-reached", self._mpv_eof_reached)

    def _setup_qtmedia(self):
        """Fallback: use QtMultimedia"""
        self._player = QMediaPlayer(self)
        self._audio_output = QAudioOutput(self)
        self._player.setAudioOutput(self._audio_output)
        self._player.setVideoOutput(self._video_widget)
        self._player.positionChanged.connect(self._qtmedia_position)
        self._player.durationChanged.connect(self._qtmedia_duration)
        self._player.mediaStatusChanged.connect(self._qtmedia_status)
        self._player.errorOccurred.connect(self._qtmedia_error)

    def _setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # Video area
        video_container = QWidget()
        video_container.setStyleSheet(f"background-color: {DARK_PALETTE['bg']};")
        video_layout = QVBoxLayout(video_container)
        video_layout.setContentsMargins(0, 0, 0, 0)

        self._player_widget = VideoPlayerWidget()
        self._video_widget = self._player_widget.video_widget()
        video_layout.addWidget(self._player_widget)

        # Connect controls
        ctrl = self._player_widget.controls()
        ctrl.playToggled.connect(self._toggle_play)
        ctrl.stopped.connect(self._stop)
        ctrl.seekRequested.connect(self._seek)
        ctrl.volumeChanged.connect(self._set_volume)
        ctrl.fullscreenToggled.connect(self._toggle_fullscreen)
        ctrl.fileOpened.connect(self._open_file_dialog)
        ctrl.playlistToggled.connect(self._toggle_playlist)

        main_layout.addWidget(video_container)

        # Playlist sidebar
        self._playlist_widget = PlaylistWidget()
        self._playlist_widget.fileSelected.connect(self._play_file)
        self._playlist_widget.fileRemoved.connect(self._remove_from_playlist)
        self._playlist_widget.cleared.connect(self._playlist_cleared)
        self._playlist_widget.hide()
        main_layout.addWidget(self._playlist_widget)

    def _setup_shortcuts(self):
        QShortcut(QKeySequence("Space"), self, self._toggle_play)
        QShortcut(QKeySequence("f"), self, self._toggle_fullscreen)
        QShortcut(QKeySequence("F"), self, self._toggle_fullscreen)
        QShortcut(QKeySequence("Escape"), self, self._exit_fullscreen)
        QShortcut(QKeySequence("o"), self, self._open_file_dialog)
        QShortcut(QKeySequence("O"), self, self._open_file_dialog)
        QShortcut(QKeySequence(Qt.Key.Key_Right), self, self._seek_forward)
        QShortcut(QKeySequence(Qt.Key.Key_Left), self, self._seek_backward)
        QShortcut(QKeySequence(Qt.Key.Key_Up), self, self._volume_up)
        QShortcut(QKeySequence(Qt.Key.Key_Down), self, self._volume_down)
        QShortcut(QKeySequence(Qt.Modifier.CTRL | Qt.Key.Key_O), self, self._open_file_dialog)
        QShortcut(QKeySequence(Qt.Modifier.CTRL | Qt.Key.Key_Q), self, self.close)

    def _apply_dark_theme(self):
        palette = QPalette()
        palette.setColor(QPalette.ColorRole.Window, QColor(DARK_PALETTE['bg']))
        palette.setColor(QPalette.ColorRole.WindowText, QColor(DARK_PALETTE['text']))
        palette.setColor(QPalette.ColorRole.Base, QColor(DARK_PALETTE['bg2']))
        palette.setColor(QPalette.ColorRole.AlternateBase, QColor(DARK_PALETTE['bg3']))
        palette.setColor(QPalette.ColorRole.ToolTipBase, QColor(DARK_PALETTE['surface']))
        palette.setColor(QPalette.ColorRole.ToolTipText, QColor(DARK_PALETTE['text']))
        palette.setColor(QPalette.ColorRole.Text, QColor(DARK_PALETTE['text']))
        palette.setColor(QPalette.ColorRole.Button, QColor(DARK_PALETTE['surface']))
        palette.setColor(QPalette.ColorRole.ButtonText, QColor(DARK_PALETTE['text']))
        palette.setColor(QPalette.ColorRole.BrightText, QColor(DARK_PALETTE['text_bright']))
        palette.setColor(QPalette.ColorRole.Link, QColor(DARK_PALETTE['accent']))
        palette.setColor(QPalette.ColorRole.Highlight, QColor(DARK_PALETTE['accent']))
        palette.setColor(QPalette.ColorRole.HighlightedText, QColor(DARK_PALETTE['text_bright']))
        self.setPalette(palette)
        self.setStyleSheet(STYLESHEET)

    # ---- MPV callbacks ----

    def _mpv_time_changed(self, name, value):
        if value is not None:
            self._player_widget.controls().set_time(value, self._duration)
            if self._duration > 0:
                self._player_widget.controls().seek_slider.setValue(int((value / self._duration) * 1000))

    def _mpv_duration_changed(self, name, value):
        if value is not None:
            self._duration = value

    def _mpv_pause_changed(self, name, value):
        self._is_playing = not value
        self._player_widget.controls().set_playing(self._is_playing)
        self._update_timer.start()

    def _mpv_eof_reached(self, name, value):
        if value:
            self._next_in_playlist()

    # ---- QtMultimedia callbacks ----

    def _qtmedia_position(self, pos):
        secs = pos / 1000
        self._player_widget.controls().set_time(secs, self._duration)
        if self._duration > 0:
            self._player_widget.controls().seek_slider.setValue(int((secs / self._duration) * 1000))

    def _qtmedia_duration(self, dur):
        self._duration = dur / 1000

    def _qtmedia_status(self, status):
        if status == QMediaPlayer.MediaStatus.EndOfMedia:
            self._next_in_playlist()

    def _qtmedia_error(self, error, msg):
        print(f"QtMedia error: {msg}")

    # ---- Playback ----

    def _toggle_play(self):
        if self._use_mpv:
            if self._mpv_player:
                self._mpv_player.pause = not self._mpv_player.pause
        else:
            if self._player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
                self._player.pause()
            else:
                self._player.play()

    def _stop(self):
        if self._use_mpv and self._mpv_player:
            self._mpv_player.stop()
        else:
            self._player.stop()

    def _seek(self, fraction):
        target = fraction * self._duration
        if self._use_mpv and self._mpv_player:
            self._mpv_player.playback_time = target
        else:
            self._player.setPosition(int(target * 1000))

    def _set_volume(self, vol):
        if self._use_mpv and self._mpv_player:
            self._mpv_player.volume = int(vol * 100)
        else:
            self._audio_output.setVolume(vol)

    def _seek_forward(self):
        if self._use_mpv and self._mpv_player:
            self._mpv_player.playback_time = min(
                self._mpv_player.playback_time + 10, self._duration
            )
        elif self._player:
            self._player.setPosition(
                min(self._player.position() + 10000, self._player.duration())
            )

    def _seek_backward(self):
        if self._use_mpv and self._mpv_player:
            self._mpv_player.playback_time = max(
                self._mpv_player.playback_time - 10, 0
            )
        elif self._player:
            self._player.setPosition(
                max(self._player.position() - 10000, 0)
            )

    def _volume_up(self):
        vol = self._player_widget.controls().vol_slider
        vol.setValue(min(vol.value() + 10, 100))

    def _volume_down(self):
        vol = self._player_widget.controls().vol_slider
        vol.setValue(max(vol.value() - 10, 0))

    # ---- Playlist ----

    def _play_file(self, path):
        idx = self._playlist_paths.index(path) if path in self._playlist_paths else -1
        if idx >= 0:
            self._current_index = idx
            self._playlist_widget.select_index(idx)
        self._open_path(path)

    def _open_path(self, path):
        if self._use_mpv:
            if self._mpv_player:
                cmd = ["loadfile", path, "replace"]
                self._mpv_player.command(*cmd)
        else:
            self._player.setSource(QUrl.fromLocalFile(path))
            self._player.play()

    def _open_file_dialog(self):
        files, _ = QFileDialog.getOpenFileNames(
            self, "Open Media Files", "",
            "Media Files (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.wmv *.mpg *.m4v "
            "*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.opus);;All Files (*)"
        )
        for f in files:
            self._playlist_paths.append(f)
            self._playlist_widget.add_file(f)
            if len(self._playlist_paths) == 1:
                self._current_index = 0
                self._open_path(f)

    def _remove_from_playlist(self, idx):
        if 0 <= idx < len(self._playlist_paths):
            self._playlist_paths.pop(idx)
            if self._current_index >= len(self._playlist_paths):
                self._current_index = max(0, len(self._playlist_paths) - 1)

    def _playlist_cleared(self):
        self._playlist_paths.clear()
        self._current_index = -1
        self._stop()

    def _next_in_playlist(self):
        if self._playlist_paths and self._current_index >= 0:
            self._current_index = (self._current_index + 1) % len(self._playlist_paths)
            self._playlist_widget.select_index(self._current_index)
            self._open_path(self._playlist_paths[self._current_index])

    def _toggle_playlist(self):
        visible = not self._playlist_widget.isVisible()
        self._playlist_widget.setVisible(visible)

    # ---- Fullscreen ----

    def _toggle_fullscreen(self):
        if self._is_fullscreen:
            self._exit_fullscreen()
        else:
            self.setWindowState(self.windowState() | Qt.WindowState.WindowFullScreen)
            self._is_fullscreen = True

    def _exit_fullscreen(self):
        self.setWindowState(self.windowState() & ~Qt.WindowState.WindowFullScreen)
        self._is_fullscreen = False

    # ---- UI update ----

    def _update_ui(self):
        if self._use_mpv and self._mpv_player:
            try:
                playing = not self._mpv_player.pause
                self._player_widget.controls().set_playing(playing)
            except:
                pass

    def closeEvent(self, event):
        if self._use_mpv and self._mpv_player:
            try:
                self._mpv_player.terminate()
            except:
                pass
        else:
            self._player.stop()
        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)

    # Enable dark title bar on Wayland if possible
    if "WAYLAND_DISPLAY" in os.environ:
        try:
            from PyQt6.QtGui import QGuiApplication
            if hasattr(QGuiApplication, 'setDesktopFileName'):
                QGuiApplication.setDesktopFileName("jnet-video-player")
        except:
            pass

    window = MainWindow()
    window.show()

    # Open files from command line
    if len(sys.argv) > 1:
        for path in sys.argv[1:]:
            if os.path.exists(path):
                window._playlist_paths.append(path)
                window._playlist_widget.add_file(path)
        if window._playlist_paths:
            window._current_index = 0
            window._open_path(window._playlist_paths[0])

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
