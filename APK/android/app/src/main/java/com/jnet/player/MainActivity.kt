package com.jnet.player

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.MediaStore
import android.view.*
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.ViewModelProvider
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import com.jnet.player.databinding.ActivityMainBinding
import com.jnet.player.databinding.ItemPlaylistBinding
import java.io.File
import java.util.concurrent.TimeUnit

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var viewModel: PlayerViewModel
    private var player: ExoPlayer? = null
    private lateinit var playlistAdapter: PlaylistAdapter
    private val handler = Handler(Looper.getMainLooper())
    private var hideControlsRunnable: Runnable? = null
    private var isFullscreen = false

    private val updateProgressRunnable = object : Runnable {
        override fun run() {
            player?.let { p ->
                updateProgress(p.currentPosition, p.duration)
            }
            handler.postDelayed(this, 500)
        }
    }

    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        uri?.let { addFileToPlaylist(it) }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ -> }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupImmersiveMode()
        setupPlayer()
        setupControls()
        setupPlaylist()
        requestPermissions()
        handleIntent(intent)
    }

    private fun setupImmersiveMode() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, binding.root)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    private fun setupPlayer() {
        player = ExoPlayer.Builder(this).build().also { exo ->
            binding.playerView.player = exo
            binding.playerView.setShowBuffering(androidx.media3.ui.PlayerView.SHOW_BUFFERING_WHEN_PLAYING)
            binding.playerView.controllerShowTimeoutMs = 3000
            binding.playerView.setShowNextButton(false)
            binding.playerView.setShowPreviousButton(false)

            exo.addListener(object : Player.Listener {
                override fun onPlaybackStateChanged(state: Int) {
                    when (state) {
                        Player.STATE_ENDED -> {
                            viewModel.onPlaybackEnded()
                        }
                        Player.STATE_READY -> {
                            exo.duration.let { d -> 
                                if (d > 0) updateDuration(d)
                            }
                        }
                    }
                }

                override fun onIsPlayingChanged(isPlaying: Boolean) {
                    viewModel.isPlaying.value = isPlaying
                    updatePlayPauseButton(isPlaying)
                    if (isPlaying) {
                        handler.post(updateProgressRunnable)
                    } else {
                        handler.removeCallbacks(updateProgressRunnable)
                    }
                }
            })
        }

        viewModel = ViewModelProvider(this)[PlayerViewModel::class.java]
    }

    private fun setupControls() {
        binding.btnPlayPause.setOnClickListener { togglePlayPause() }
        binding.btnStop.setOnClickListener { stopPlayback() }
        binding.btnPrev.setOnClickListener { viewModel.playPrevious() }
        binding.btnNext.setOnClickListener { viewModel.playNext() }
        binding.btnOpen.setOnClickListener { openFilePicker() }
        binding.btnPlaylist.setOnClickListener { togglePlaylist() }
        binding.btnFullscreen.setOnClickListener { toggleFullscreen() }

        binding.seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    player?.duration?.let { dur ->
                        val pos = (progress / 1000f * dur).toLong()
                        binding.tvTime.text = formatTime(pos) + " / " + formatTime(dur)
                    }
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) {
                handler.removeCallbacks(updateProgressRunnable)
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                player?.duration?.let { dur ->
                    val pos = (seekBar.progress / 1000f * dur).toLong()
                    player?.seekTo(pos)
                }
                if (player?.isPlaying == true) {
                    handler.post(updateProgressRunnable)
                }
            }
        })

        binding.volumeSlider.addOnChangeListener { _, value, fromUser ->
            if (fromUser) {
                player?.volume = value
                binding.tvVolume.text = "${(value * 100).toInt()}%"
            }
        }

        // Touch to show/hide controls
        binding.playerView.setOnClickListener {
            binding.playerView.performClick()
        }
    }

    private fun setupPlaylist() {
        playlistAdapter = PlaylistAdapter(
            onItemClick = { pos -> viewModel.playAt(pos) },
            onItemRemove = { pos -> viewModel.removeAt(pos) }
        )
        binding.playlistRecyclerView.adapter = playlistAdapter

        viewModel.playlist.observe(this) { items ->
            playlistAdapter.submitList(items.toList())
            binding.tvPlaylistCount.text = "${items.size} files"
        }

        viewModel.currentIndex.observe(this) { idx ->
            playlistAdapter.setCurrentIndex(idx)
            binding.playlistRecyclerView.smoothScrollToPosition(idx)
        }
    }

    private fun requestPermissions() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_MEDIA_VIDEO)
                != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.READ_MEDIA_VIDEO)
            }
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.READ_EXTERNAL_STORAGE)
            }
        }
        if (permissions.isNotEmpty()) {
            permissionLauncher.launch(permissions.toTypedArray())
        }
    }

    private fun handleIntent(intent: Intent?) {
        intent?.data?.let { uri ->
            addFileToPlaylist(uri)
            viewModel.clearPlaylist()
            viewModel.addToPlaylist(uri)
        }
    }

    private fun addFileToPlaylist(uri: Uri) {
        val name = getFileName(uri) ?: "Unknown"
        val item = PlaylistItem(name, uri.toString())
        viewModel.addToPlaylist(item)
    }

    private fun getFileName(uri: Uri): String? {
        var name: String? = null
        contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val idx = cursor.getColumnIndex(MediaStore.MediaColumns.DISPLAY_NAME)
                if (idx >= 0) name = cursor.getString(idx)
            }
        }
        return name ?: uri.lastPathSegment
    }

    private fun openFilePicker() {
        val mimeTypes = arrayOf("video/*", "audio/*")
        filePickerLauncher.launch(mimeTypes)
    }

    private fun togglePlayPause() {
        player?.let { p ->
            if (p.isPlaying) p.pause() else p.play()
        }
    }

    private fun stopPlayback() {
        player?.stop()
        player?.clearMediaItems()
        viewModel.clearPlaylist()
    }

    private fun updatePlayPauseButton(isPlaying: Boolean) {
        binding.btnPlayPause.text = if (isPlaying) "⏸" else "▶"
    }

    private fun updateProgress(position: Long, duration: Long) {
        if (duration > 0) {
            val progress = ((position.toFloat() / duration) * 1000).toInt()
            binding.seekBar.progress = progress
            binding.tvTime.text = formatTime(position) + " / " + formatTime(duration)
        }
    }

    private fun updateDuration(duration: Long) {
        binding.seekBar.max = 1000
    }

    private fun formatTime(ms: Long): String {
        val hours = TimeUnit.MILLISECONDS.toHours(ms)
        val minutes = TimeUnit.MILLISECONDS.toMinutes(ms) % 60
        val seconds = TimeUnit.MILLISECONDS.toSeconds(ms) % 60
        return if (hours > 0) {
            String.format("%d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format("%02d:%02d", minutes, seconds)
        }
    }

    private fun togglePlaylist() {
        val isVisible = binding.playlistContainer.visibility == View.VISIBLE
        binding.playlistContainer.visibility = if (isVisible) View.GONE else View.VISIBLE
        binding.btnPlaylist.isSelected = !isVisible
    }

    private fun toggleFullscreen() {
        isFullscreen = !isFullscreen
        val controller = WindowInsetsControllerCompat(window, binding.root)
        if (isFullscreen) {
            controller.hide(WindowInsetsCompat.Type.systemBars())
            binding.controlsContainer.visibility = View.GONE
        } else {
            controller.show(WindowInsetsCompat.Type.systemBars())
            binding.controlsContainer.visibility = View.VISIBLE
        }
    }

    override fun onStart() {
        super.onStart()
        viewModel.currentItem.observe(this) { item ->
            item?.let {
                player?.let { p ->
                    val mediaItem = MediaItem.fromUri(it.uri)
                    p.setMediaItem(mediaItem)
                    p.prepare()
                    p.play()
                }
            }
        }
    }

    override fun onStop() {
        super.onStop()
        player?.pause()
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(updateProgressRunnable)
        player?.release()
        player = null
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        player?.let { p ->
            when (keyCode) {
                KeyEvent.KEYCODE_SPACE -> { togglePlayPause(); return true }
                KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE -> { togglePlayPause(); return true }
                KeyEvent.KEYCODE_MEDIA_PLAY -> { if (!p.isPlaying) p.play(); return true }
                KeyEvent.KEYCODE_MEDIA_PAUSE -> { if (p.isPlaying) p.pause(); return true }
                KeyEvent.KEYCODE_MEDIA_STOP -> { stopPlayback(); return true }
                KeyEvent.KEYCODE_MEDIA_NEXT -> { viewModel.playNext(); return true }
                KeyEvent.KEYCODE_MEDIA_PREVIOUS -> { viewModel.playPrevious(); return true }
                KeyEvent.KEYCODE_F -> { toggleFullscreen(); return true }
                KeyEvent.KEYCODE_MENU -> { togglePlaylist(); return true }
                KeyEvent.KEYCODE_VOLUME_UP -> { 
                    p.volume = (p.volume + 0.1f).coerceAtMost(1f)
                    binding.volumeSlider.value = p.volume
                    return true
                }
                KeyEvent.KEYCODE_VOLUME_DOWN -> { 
                    p.volume = (p.volume - 0.1f).coerceAtLeast(0f)
                    binding.volumeSlider.value = p.volume
                    return true
                }
                KeyEvent.KEYCODE_DPAD_LEFT -> { 
                    p.seekTo((p.currentPosition - 10000).coerceAtLeast(0))
                    return true
                }
                KeyEvent.KEYCODE_DPAD_RIGHT -> { 
                    p.seekTo((p.currentPosition + 10000).coerceAtMost(p.duration))
                    return true
                }
            }
        }
        return super.onKeyDown(keyCode, event)
    }
}
