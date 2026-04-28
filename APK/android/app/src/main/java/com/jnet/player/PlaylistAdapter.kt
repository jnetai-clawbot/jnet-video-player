package com.jnet.player

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.jnet.player.databinding.ItemPlaylistBinding

class PlaylistAdapter(
    private val onItemClick: (Int) -> Unit,
    private val onItemRemove: (Int) -> Unit
) : ListAdapter<PlaylistItem, PlaylistAdapter.ViewHolder>(PlaylistDiffCallback()) {

    private var currentIndex: Int = -1

    fun setCurrentIndex(index: Int) {
        val oldIndex = currentIndex
        currentIndex = index
        if (oldIndex >= 0) notifyItemChanged(oldIndex)
        if (index >= 0) notifyItemChanged(index)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemPlaylistBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = getItem(position)
        holder.bind(item, position == currentIndex)
        holder.itemView.setOnClickListener { onItemClick(position) }
        holder.itemView.setOnLongClickListener {
            onItemRemove(position)
            true
        }
    }

    class ViewHolder(
        private val binding: ItemPlaylistBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(item: PlaylistItem, isPlaying: Boolean) {
            binding.tvFileName.text = item.name
            binding.tvFileName.alpha = if (isPlaying) 1.0f else 0.8f
            
            if (isPlaying) {
                binding.root.setBackgroundResource(R.drawable.bg_playlist_selected)
                binding.tvNowPlaying.visibility = android.view.View.VISIBLE
            } else {
                binding.root.setBackgroundResource(0)
                binding.tvNowPlaying.visibility = android.view.View.GONE
            }
        }
    }

    class PlaylistDiffCallback : DiffUtil.ItemCallback<PlaylistItem>() {
        override fun areItemsTheSame(oldItem: PlaylistItem, newItem: PlaylistItem): Boolean {
            return oldItem.uri == newItem.uri
        }

        override fun areContentsTheSame(oldItem: PlaylistItem, newItem: PlaylistItem): Boolean {
            return oldItem == newItem
        }
    }
}
