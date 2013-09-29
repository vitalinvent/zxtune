/*
 * @file
 * @brief Remote server stub for Playback.Control
 * @version $Id:$
 * @author (C) Vitamin/CAIG
 */

package app.zxtune.rpc;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

import android.net.Uri;
import android.os.RemoteException;
import android.util.Log;
import app.zxtune.TimeStamp;
import app.zxtune.playback.Callback;
import app.zxtune.playback.Item;
import app.zxtune.playback.PlaybackControl;
import app.zxtune.playback.PlaybackService;
import app.zxtune.playback.PlaylistControl;
import app.zxtune.playback.SeekControl;
import app.zxtune.playback.Visualizer;

public class PlaybackServiceServer extends IRemotePlaybackService.Stub {

  private final PlaybackService delegate;
  private final PlaylistControl playlist;
  private final PlaybackControl playback;
  private final SeekControl seek;
  private final Visualizer visualizer;
  private final HashMap<IRemoteCallback, Callback> callbacks;

  public PlaybackServiceServer(PlaybackService delegate) {
    this.delegate = delegate;
    this.playlist = delegate.getPlaylistControl();
    this.playback = delegate.getPlaybackControl();
    this.seek = delegate.getSeekControl();
    this.visualizer = delegate.getVisualizer();
    this.callbacks = new HashMap<IRemoteCallback, Callback>();
  }

  @Override
  public ParcelablePlaybackItem getNowPlaying() {
    return ParcelablePlaybackItem.create(delegate.getNowPlaying());
  }
  
  @Override
  public void setNowPlaying(Uri[] uris) {
    delegate.setNowPlaying(uris);
  }
  
  @Override
  public void add(Uri[] uris) {
    playlist.add(uris);
  }
  
  @Override
  public void delete(long[] ids) {
    playlist.delete(ids);
  }
  
  @Override
  public void deleteAll() {
    playlist.deleteAll();
  }

  @Override
  public void play() {
    playback.play();
  }
  
  @Override
  public void stop() {
    playback.stop();
  }
  
  @Override
  public boolean isPlaying() {
    return playback.isPlaying();
  }
    
  @Override
  public void next() {
    playback.next();
  }
  
  @Override
  public void prev() {
    playback.prev();
  }
  
  @Override
  public long getDuration() {
    return seek.getDuration().convertTo(TimeUnit.MILLISECONDS);
  }
  
  @Override
  public long getPosition() {
    return seek.getPosition().convertTo(TimeUnit.MILLISECONDS);
  }
  
  @Override
  public void setPosition(long ms) {
    seek.setPosition(TimeStamp.createFrom(ms, TimeUnit.MILLISECONDS));
  }
  
  @Override
  public int[] getSpectrum() {
    final int[] bands = new int[16];
    final int[] levels = new int[16];
    final int chans = visualizer.getSpectrum(bands, levels);
    final int[] res = new int[chans];
    for (int idx = 0; idx != chans; ++idx) {
      res[idx] = (levels[idx] << 8) | bands[idx];
    }
    return res;
  }
  
  @Override
  public void subscribe(IRemoteCallback callback) {
    final Callback client = new CallbackClient(callback);
    callbacks.put(callback, client);
    delegate.subscribe(client);
  }
  
  @Override
  public void unsubscribe(IRemoteCallback callback) {
    final Callback client = callbacks.get(callback);
    if (client != null) {
      delegate.unsubscribe(client);
      callbacks.remove(callback);
    }
  }
  
  private final class CallbackClient implements Callback {
    
    private final String TAG = CallbackClient.class.getName();
    private final IRemoteCallback delegate;
    
    public CallbackClient(IRemoteCallback delegate) {
      this.delegate = delegate;
    }
    
    @Override
    public void onStatusChanged(boolean isPlaying) {
      try {
        delegate.onStatusChanged(isPlaying);
      } catch (RemoteException e) {
        Log.e(TAG, "onStatusChanged()", e);
      }
    }
    
    @Override
    public void onItemChanged(Item item) {
      try {
        delegate.onItemChanged(ParcelablePlaybackItem.create(item));
      } catch (RemoteException e) {
        Log.e(TAG, "onItemChanged()", e);
      }
    }
  }
}
