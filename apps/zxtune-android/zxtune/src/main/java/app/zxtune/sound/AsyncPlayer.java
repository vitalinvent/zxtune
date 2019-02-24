/**
 * @file
 * @brief Asynchronous implementation of Player
 * @author vitamin.caig@gmail.com
 */

package app.zxtune.sound;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import android.support.annotation.NonNull;
import app.zxtune.Log;

public final class AsyncPlayer implements Player {

  private static final String TAG = AsyncPlayer.class.getName();

  private static final int UNINITIALIZED = -1;
  private static final int STOPPED = 0;
  private static final int STARTING = 1;
  private static final int STARTED = 2;
  private static final int STOPPING = 3;
  private static final int FINISHING = 4;

  private final PlayerEventsListener events;
  private final AtomicInteger state;
  private final AtomicReference<SamplesSource> source;
  private AsyncSamplesTarget target;
  private Thread thread;

  public static AsyncPlayer create(SamplesTarget target, PlayerEventsListener events) {
    return new AsyncPlayer(target, events);
  }

  private AsyncPlayer(SamplesTarget target, PlayerEventsListener events) {
    this.events = events;
    this.state = new AtomicInteger(UNINITIALIZED);
    this.source = new AtomicReference<>(StubSamplesSource.instance());
    this.target = new AsyncSamplesTarget(target);
  }

  public final int getSampleRate() {
    return target.getSampleRate();
  }

  @Override
  public void setSource(@NonNull SamplesSource src) throws Exception {
    src.initialize(target.getSampleRate());
    synchronized(state) {
      source.set(src);
      state.compareAndSet(UNINITIALIZED, STOPPED);
      state.notify();
    }
  }

  @Override
  public void startPlayback() {
    if (state.compareAndSet(STOPPED, STARTING)) {
      thread = new Thread("PlayerThread") {
        @Override
        public void run() {
          syncPlay();
        }
      };
      thread.start();
    } else if (state.compareAndSet(FINISHING, STARTED)) {
      //replay
      synchronized(state) {
        state.notify();
      }
    }
  }

  private void syncPlay() {
    try {
      target.start();
      try {
        Log.d(TAG, "Start transfer cycle");
        transferCycle();
      } finally {
        Log.d(TAG, "Stop transfer cycle");
        target.stop();
      }
    } catch (Exception e) {
      Log.w(TAG, new Exception(e), "Playback initialization failed");
      events.onError(e);
      state.set(STOPPED);
    }
  }

  @Override
  public void stopPlayback() {
    if (state.compareAndSet(STARTED, STOPPING)) {
      while (true) {
        try {
          thread.join();
          break;
        } catch (InterruptedException e) {
          Log.w(TAG, new Exception(e), "Interrupted while stopping");
        }
        thread.interrupt();
      }
      thread = null;
    }
  }

  @Override
  public boolean isStarted() {
    return state.compareAndSet(STARTED, STARTED);
  }

  @Override
  public void release() {
    source.set(null);
    target.release();
    target = null;
  }

  private void transferCycle() {
    state.set(STARTED);
    events.onStart();
    try {
      while (isStarted()) {
        final short[] buf = target.getBuffer();
        if (getSamples(buf)) {
          if (!commitSamples()) {
            break;
          }
        } else {
          events.onFinish();
          if (!waitForNextSource()) {
            Log.d(TAG, "No next source, exiting");
            break;
          }
        }
      }
    } catch (InterruptedException e) {
      if (isStarted()) {
        Log.w(TAG, new Exception(e),"Interrupted transfer cycle");
      }
    } catch (Exception e) {
      events.onError(e);
    } finally {
      state.set(STOPPED);
      events.onStop();
    }
  }

  private boolean getSamples(short[] buf) throws Exception {
    return source.get().getSamples(buf);
  }

  private boolean commitSamples() throws Exception {
    while (!target.commitBuffer()) {//interruption point
      if (!isStarted()) {
        return false;
      }
    }
    return true;
  }

  private boolean waitForNextSource() throws InterruptedException {
    state.set(FINISHING);
    events.onStop();
    final int NEXT_SOURCE_WAIT_SECONDS = 5;
    synchronized(state) {
      final SamplesSource prevSource = source.get();
      state.wait(NEXT_SOURCE_WAIT_SECONDS * 1000);
      if (isStarted()) {
        //replay
        events.onStart();
        return true;
      } else if (source.compareAndSet(prevSource, prevSource)) {
        return false;//same finished source
      } else {
        state.set(STARTED);
        events.onStart();
        return true;
      }
    }
  }
}
