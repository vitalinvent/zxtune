package app.zxtune.analytics;

import android.content.Context;
import android.net.Uri;

import app.zxtune.Log;
import app.zxtune.analytics.internal.Factory;
import app.zxtune.analytics.internal.UrlsBuilder;
import app.zxtune.analytics.internal.UrlsSink;
import app.zxtune.core.Module;
import app.zxtune.core.ModuleAttributes;
import app.zxtune.core.Player;
import app.zxtune.playback.PlayableItem;
import app.zxtune.playlist.ProviderClient;

import java.util.concurrent.TimeUnit;

import static app.zxtune.analytics.internal.UrlsBuilder.DEFAULT_LONG_VALUE;
import static app.zxtune.analytics.internal.UrlsBuilder.DEFAULT_STRING_VALUE;

final class InternalSink implements Sink {

  private static final String TAG = InternalSink.class.getName();

  private final UrlsSink delegate;

  InternalSink(Context ctx) {
    this.delegate = Factory.createClientEndpoint(ctx);
  }

  @Override
  public void logException(Throwable e) {

  }

  @Override
  public void sendPlayEvent(PlayableItem item, Player player) {
    final UrlsBuilder builder = new UrlsBuilder("track/done");
    builder.addUri(item.getDataId().getFullLocation());
    builder.addParam("from", item.getId().getScheme());
    final Module mod = item.getModule();

    builder.addParam("type", mod.getProperty(ModuleAttributes.TYPE, DEFAULT_STRING_VALUE));
    builder.addParam("container", mod.getProperty(ModuleAttributes.CONTAINER, DEFAULT_STRING_VALUE));
    builder.addParam("duration", item.getDuration().convertTo(TimeUnit.SECONDS));

    builder.addParam("crc", mod.getProperty("CRC", DEFAULT_LONG_VALUE));
    builder.addParam("fixedcrc", mod.getProperty("FixedCRC", DEFAULT_LONG_VALUE));
    builder.addParam("size", mod.getProperty("Size", DEFAULT_LONG_VALUE));
    builder.addParam("perf", player.getPerformance());
    builder.addParam("progress", player.getProgress());

    send(builder);
  }

  @Override
  public void sendBrowserEvent(Uri path, @Analytics.BrowserAction int action) {
    final UrlsBuilder builder = new UrlsBuilder("ui/browser/" + serializeBrowserAction(action));
    builder.addUri(path);

    send(builder);
  }

  private static String serializeBrowserAction(@Analytics.BrowserAction int action) {
    switch (action) {
      case Analytics.BROWSER_ACTION_BROWSE:
        return "browse";
      case Analytics.BROWSER_ACTION_BROWSE_PARENT:
        return "up";
      case Analytics.BROWSER_ACTION_SEARCH:
        return "search";
      default:
        return "";
    }
  }

  @Override
  public void sendSocialEvent(Uri path, String app, @Analytics.SocialAction int action) {
    final UrlsBuilder builder = new UrlsBuilder("ui/" + serializeSocialAction(action));
    builder.addUri(path);

    send(builder);
  }

  private static String serializeSocialAction(@Analytics.SocialAction int action) {
    switch (action) {
      case Analytics.SOCIAL_ACTION_RINGTONE:
        return "ringtone";
      case Analytics.SOCIAL_ACTION_SHARE:
        return "share";
      case Analytics.SOCIAL_ACTION_SEND:
        return "send";
      default:
        return "";
    }
  }

  @Override
  public void sendUiEvent(@Analytics.UiAction int action) {
    final String actionStr = serializeUiAction(action);
    if (!actionStr.isEmpty()) {
      final UrlsBuilder builder = new UrlsBuilder("ui/" + actionStr);

      send(builder);
    }
  }

  private static String serializeUiAction(@Analytics.UiAction int action) {
    switch (action) {
      case Analytics.UI_ACTION_OPEN:
        return "open";
      case Analytics.UI_ACTION_CLOSE:
        return "close";
      case Analytics.UI_ACTION_PREFERENCES:
        return "preferences";
      case Analytics.UI_ACTION_RATE:
        return "rate";
      case Analytics.UI_ACTION_ABOUT:
        return "about";
      case Analytics.UI_ACTION_QUIT:
        return "quit";
      default:
        return "";
    }
  }

  @Override
  public void sendPlaylistEvent(@Analytics.PlaylistAction int action, int param) {
    final UrlsBuilder builder = new UrlsBuilder("ui/playlist/" + serializePlaylistAction(action));
    if (action == Analytics.PLAYLIST_ACTION_SORT) {
      builder.addParam("by", ProviderClient.SortBy.values()[param / 100].name());
      builder.addParam("order", ProviderClient.SortOrder.values()[param % 100].name());
    } else {
      builder.addParam("count", param != 0 ? param : DEFAULT_LONG_VALUE);
    }

    send(builder);
  }

  private static String serializePlaylistAction(@Analytics.PlaylistAction int action) {
    switch (action) {
      case Analytics.PLAYLIST_ACTION_ADD:
        return "add";
      case Analytics.PLAYLIST_ACTION_DELETE:
        return "delete";
      case Analytics.PLAYLIST_ACTION_MOVE:
        return "move";
      case Analytics.PLAYLIST_ACTION_SORT:
        return "sort";
      case Analytics.PLAYLIST_ACTION_SAVE:
        return "save";
      case Analytics.PLAYLIST_ACTION_STATISTICS:
        return "statistics";
      default:
        return "";
    }
  }

  @Override
  public void sendVfsEvent(String id, String scope, @Analytics.VfsAction int action) {
    final UrlsBuilder builder = new UrlsBuilder("vfs/" + serializeVfsAction(action));
    builder.addParam("id", id);
    builder.addParam("scope", scope);

    send(builder);
  }

  private static String serializeVfsAction(@Analytics.VfsAction int action) {
    switch (action) {
      case Analytics.VFS_ACTION_REMOTE_FETCH:
        return "remote";
      case Analytics.VFS_ACTION_REMOTE_FALLBACK:
        return "remote_fallback";
      case Analytics.VFS_ACTION_CACHED_FETCH:
        return "cache";
      case Analytics.VFS_ACTION_CACHED_FALLBACK:
        return "cache_fallback";
      default:
        return "";
    }
  }

  @Override
  public void sendNoTracksFoundEvent(Uri uri) {
    final UrlsBuilder builder = new UrlsBuilder("track/notfound");
    builder.addUri(uri);
    if ("file".equals(uri.getScheme())) {
      final String filename = uri.getLastPathSegment();
      final int extPos = filename != null ? filename.lastIndexOf('.') : -1;
      final String type = extPos != -1 ? filename.substring(extPos + 1) : "none";
      builder.addParam("type", type);
    }

    send(builder);
  }

  @Override
  public void sendHostUnavailableEvent(String host) {
  }

  private void send(UrlsBuilder builder) {
    try {
      delegate.push(builder.getResult());
    } catch (Exception e) {
      Log.w(TAG, e, "Failed to send event");
    }
  }
}
