package com.jnet.player

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData

data class PlaylistItem(
    val name: String,
    val uri: String,
    var isPlaying: Boolean = false
)

class PlayerViewModel(application: Application) : AndroidViewModel(application) {

    private val _playlist = MutableLiveData<MutableList<PlaylistItem>>(mutableListOf())
    val playlist: LiveData<MutableList<PlaylistItem>> = _playlist

    private val _currentIndex = MutableLiveData(-1)
    val currentIndex: LiveData<Int> = _currentIndex

    private val _currentItem = MutableLiveData<PlaylistItem?>()
    val currentItem: LiveData<PlaylistItem?> = _currentItem

    private val _isPlaying = MutableLiveData(false)
    val isPlaying: LiveData<Boolean> = _isPlaying

    fun setPlaying(playing: Boolean) {
        _isPlaying.value = playing
    }

    fun addToPlaylist(item: PlaylistItem) {
        val list = _playlist.value ?: mutableListOf()
        list.add(item)
        _playlist.value = list
        
        if (_currentIndex.value == -1) {
            _currentIndex.value = 0
            _currentItem.value = item
        }
    }

    fun addToPlaylist(uri: android.net.Uri) {
        val name = uri.lastPathSegment ?: "Unknown"
        addToPlaylist(PlaylistItem(name, uri.toString()))
    }

    fun removeAt(index: Int) {
        val list = _playlist.value ?: return
        if (index < 0 || index >= list.size) return
        
        list.removeAt(index)
        _playlist.value = list
        
        when {
            list.isEmpty() -> {
                _currentIndex.value = -1
                _currentItem.value = null
            }
            index == _currentIndex.value -> {
                if (index >= list.size) {
                    _currentIndex.value = list.size - 1
                }
                _currentItem.value = list.getOrNull(_currentIndex.value ?: 0)
            }
            index < (_currentIndex.value ?: 0) -> {
                _currentIndex.value = (_currentIndex.value ?: 1) - 1
            }
        }
    }

    fun clearPlaylist() {
        _playlist.value = mutableListOf()
        _currentIndex.value = -1
        _currentItem.value = null
    }

    fun playAt(index: Int) {
        val list = _playlist.value ?: return
        if (index < 0 || index >= list.size) return
        
        _currentIndex.value = index
        _currentItem.value = list[index]
    }

    fun playNext() {
        val list = _playlist.value ?: return
        if (list.isEmpty()) return
        
        val next = ((_currentIndex.value ?: 0) + 1) % list.size
        playAt(next)
    }

    fun playPrevious() {
        val list = _playlist.value ?: return
        if (list.isEmpty()) return
        
        val prev = if ((_currentIndex.value ?: 0) <= 0) list.size - 1 
                   else (_currentIndex.value ?: 1) - 1
        playAt(prev)
    }

    fun onPlaybackEnded() {
        val list = _playlist.value ?: return
        if (list.size > 1) {
            playNext()
        }
    }
}
