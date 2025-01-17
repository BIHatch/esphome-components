#include "HomeAssistantBaseMediaPlayer.h"

namespace esphome {
namespace homeassistant_media_player {

static const char* const TAG = "homeassistant.media_player_base";

void HomeAssistantBaseMediaPlayer::setup() {
  ESP_LOGI(TAG, "'%s': Subscribe states", get_name().c_str());

  subscribe_player_state();
  subscribe_supported_features();
  subscribe_source();
  subscribe_media_title();
  subscribe_media_artist();
}

void HomeAssistantBaseMediaPlayer::register_source(
    media_player_source::MediaPlayerSourceBase* new_source) {
  sources.push_back(new_source);

  // new_media_player->add_on_state_callback([this, new_media_player]() {
  //   this->state_updated(new_media_player->playerState);
  // });
}

void HomeAssistantBaseMediaPlayer::playSource(
    media_player_source::MediaPlayerSourceItem* source) {
  ESP_LOGI(TAG, "playSource: %s %s %s %d", this->entity_id_.c_str(),
           source->get_media_content_id().c_str(),
           source->sourceTypeString().c_str(), source->get_media_type());
  switch (source->get_media_type()) {
    case media_player_source::MusicRemotePlayerSourceType:
    case media_player_source::FavoriteItemIDRemotePlayerSourceType:
    case media_player_source::PlaylistRemotePlayerSourceType:
      playMedia(source);
      break;
    case media_player_source::SourceRemotePlayerSourceType:
      selectSource(source);
      break;
  }
}

void HomeAssistantBaseMediaPlayer::playPause() {
  ESP_LOGI(TAG, "%s play pause", this->entity_id_.c_str());
  call_homeassistant_service("media_player.media_play_pause",
                             {
                                 {"entity_id", this->entity_id_},
                             });
}

void HomeAssistantBaseMediaPlayer::nextTrack() {
  ESP_LOGI(TAG, "%s next track", this->entity_id_.c_str());
  call_homeassistant_service("media_player.media_next_track",
                             {
                                 {"entity_id", this->entity_id_},
                             });
}

std::string HomeAssistantBaseMediaPlayer::mediaTitleString() {
  switch (get_player_type()) {
    case homeassistant_media_player::RemotePlayerType::TVRemotePlayerType: {
      if (mediaArtist != "") {
        return mediaArtist;
      }
      return playerSourceStateString(mediaSource);
    }
    case homeassistant_media_player::RemotePlayerType::SpeakerRemotePlayerType:
      return mediaArtist;
  }
  return "";
}

std::string HomeAssistantBaseMediaPlayer::mediaSubtitleString() {
  switch (get_player_type()) {
    case homeassistant_media_player::RemotePlayerType::TVRemotePlayerType: {
      if (mediaTitle != "") {
        return mediaTitle;
      }
      return playerSourceStateString(mediaSource);
    }
    case homeassistant_media_player::RemotePlayerType::SpeakerRemotePlayerType:
      return mediaTitle;
  }
  return "";
}

void HomeAssistantBaseMediaPlayer::clearMedia() {
  clearSource();
  mediaTitle = "";
  mediaArtist = "";
  playlist_title = "";
}

void HomeAssistantBaseMediaPlayer::selectSource(
    media_player_source::MediaPlayerSourceItem* source) {
  ESP_LOGI(TAG, "selectSource: %s %s %s %d", this->entity_id_.c_str(),
           source->get_media_content_id().c_str(),
           source->sourceTypeString().c_str(), source->get_media_type());
  call_homeassistant_service(
      "media_player.select_source",
      {
          {"entity_id", entity_id_},
          {"source", source->get_media_content_id().c_str()},
      });
}

void HomeAssistantBaseMediaPlayer::playMedia(
    media_player_source::MediaPlayerSourceItem* source) {
  ESP_LOGI(TAG, "playMedia: %s %s %s %d", this->entity_id_.c_str(),
           source->get_media_content_id().c_str(),
           source->sourceTypeString().c_str(), source->get_media_type());
  call_homeassistant_service(
      "media_player.play_media",
      {
          {"entity_id", this->entity_id_},
          {"media_content_id", source->get_media_content_id().c_str()},
          {"media_content_type", source->sourceTypeString().c_str()},
      });
}

void HomeAssistantBaseMediaPlayer::playerState_changed(std::string state) {
  ESP_LOGI(TAG, "playerState_changed: %s changed to %s",
           this->entity_id_.c_str(), state.c_str());
  if (state.length() == 0) {
    playerState = StoppedRemotePlayerState;
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  } else if (strcmp(state.c_str(), "playing") == 0) {
    if (playerState == PlayingRemotePlayerState) {
      clearSource();
    }
    playerState = PlayingRemotePlayerState;
    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
  } else if (strcmp(state.c_str(), "paused") == 0) {
    playerState = PausedRemotePlayerState;
    this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
  } else if (strcmp(state.c_str(), "standby") == 0) {
    playerState = PowerOffRemotePlayerState;
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    clearMedia();
  } else if (strcmp(state.c_str(), "off") == 0) {
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    playerState = PowerOffRemotePlayerState;
    clearMedia();
  } else if (strcmp(state.c_str(), "home") == 0 ||
             strcmp(state.c_str(), "Roku") == 0) {
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    playerState = StoppedRemotePlayerState;
    clearMedia();
  } else if (strcmp(state.c_str(), "on") == 0) {
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    playerState = StoppedRemotePlayerState;
  } else if (strcmp(state.c_str(), "idle") == 0) {
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    clearMedia();
    playerState = StoppedRemotePlayerState;
  } else if (strcmp(state.c_str(), "unavailable") == 0) {
    this->state = media_player::MEDIA_PLAYER_STATE_NONE;
    playerState = UnavailableRemotePlayerState;
  } else {
    this->state = media_player::MEDIA_PLAYER_STATE_NONE;
    playerState = NoRemotePlayerState;
  }
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::player_supported_features_changed(
    std::string state) {
  ESP_LOGI(TAG, "player_supported_features_changed: %s changed to %s",
           this->entity_id_.c_str(), state.c_str());

  if (playerState == NoRemotePlayerState) {
    ESP_LOGW(TAG,
             "player_supported_features_changed: %s updated without state. "
             "making it unavailable until it updates",
             this->entity_id_.c_str());
    this->state = media_player::MEDIA_PLAYER_STATE_NONE;
    playerState = UnavailableRemotePlayerState;
  }
  for (auto const& x : supported_feature_string_map) {
    if (state.find(x.second) != std::string::npos) {
      auto new_feature = std::make_shared<MediaPlayerSupportedFeature>(x.first);
      supported_features_.push_back(new_feature);
      ESP_LOGI(TAG, "%s, supported feature: %s", this->entity_id_.c_str(),
               x.second.c_str());
      switch (x.first) {
        case PAUSE:
          // subscribe_player_state();
          break;
        case VOLUME_MUTE:
          subscribe_muted();
          break;
        case SHUFFLE_SET:
          subscribe_shuffle();
          break;
        case VOLUME_SET:
          subscribe_volume();
          break;
        case SEEK:
          subscribe_media_position();
          break;
        case SELECT_SOURCE:
          subscribe_sources();
          break;
        case CLEAR_PLAYLIST:
          subscribe_playlist();
          break;
        case GROUPING:
          subscribe_group_members();
          break;
        case REPEAT_SET:
          subscribe_repeat();
          break;
        case SELECT_SOUND_MODE:
        case BROWSE_MEDIA:
        case TURN_ON:
        case TURN_OFF:
        case PLAY_MEDIA:
        case VOLUME_STEP:
        case STOP:
          break;

        // unsupported
        case PLAY:
        case PREVIOUS_TRACK:
        case NEXT_TRACK:
        default:
          break;
      }
    }
  }
}

void HomeAssistantBaseMediaPlayer::subscribe_supported_features() {
  ESP_LOGI(TAG, "subscribe_supported_features: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::player_supported_features_changed,
      this->entity_id_, "supported_features");
}

void HomeAssistantBaseMediaPlayer::subscribe_player_state() {
  ESP_LOGI(TAG, "subscribe_player_state: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::playerState_changed, this->entity_id_);
}

void HomeAssistantBaseMediaPlayer::subscribe_media_title() {
  ESP_LOGI(TAG, "subscribe_media_title: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::media_title_changed, this->entity_id_,
      "media_title");
}

void HomeAssistantBaseMediaPlayer::subscribe_media_artist() {
  ESP_LOGI(TAG, "subscribe_media_artist: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::media_artist_changed, this->entity_id_,
      "media_artist");
}

void HomeAssistantBaseMediaPlayer::subscribe_shuffle() {
  ESP_LOGI(TAG, "subscribe_shuffle: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(&HomeAssistantBaseMediaPlayer::shuffle_changed,
                                this->entity_id_, "shuffle");
}

void HomeAssistantBaseMediaPlayer::subscribe_repeat() {
  ESP_LOGI(TAG, "subscribe_repeat: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(&HomeAssistantBaseMediaPlayer::repeat_changed,
                                this->entity_id_, "repeat");
}

void HomeAssistantBaseMediaPlayer::subscribe_muted() {
  ESP_LOGI(TAG, "subscribe_muted: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(&HomeAssistantBaseMediaPlayer::muted_changed,
                                this->entity_id_, "is_volume_muted");
}

void HomeAssistantBaseMediaPlayer::subscribe_media_position() {
  ESP_LOGI(TAG, "subscribe_media_position: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::media_duration_changed, this->entity_id_,
      "media_duration");
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::media_position_changed, this->entity_id_,
      "media_position");
}

void HomeAssistantBaseMediaPlayer::subscribe_volume() {
  ESP_LOGI(TAG, "subscribe_volume: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(&HomeAssistantBaseMediaPlayer::volume_changed,
                                this->entity_id_, "volume_level");
}

void HomeAssistantBaseMediaPlayer::subscribe_playlist() {
  ESP_LOGI(TAG, "subscribe_playlist: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(&HomeAssistantBaseMediaPlayer::playlist_changed,
                                this->entity_id_, "media_playlist");
}

void HomeAssistantBaseMediaPlayer::subscribe_group_members() {
  ESP_LOGI(TAG, "subscribe_playlist: %s", this->entity_id_.c_str());
  subscribe_homeassistant_state(
      &HomeAssistantBaseMediaPlayer::group_members_changed, this->entity_id_,
      "group_members");
}

void HomeAssistantBaseMediaPlayer::media_title_changed(std::string state) {
  ESP_LOGI(TAG, "media_title_changed: %s changed to %s",
           this->entity_id_.c_str(), state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  if (strcmp(state.c_str(), mediaTitle.c_str()) != 0) {
    mediaPosition = 0;
  }
  if (strcmp("TV", state.c_str()) != 0) {
    mediaTitle = state.c_str();
  } else {
    mediaTitle = "";
    mediaArtist = "";
    playlist_title = "";
    mediaPosition = -1;
  }
  mediaDuration = -1;
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::media_artist_changed(std::string state) {
  ESP_LOGI(TAG, "media_artist_changed: %s changed to %s",
           this->entity_id_.c_str(), state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  mediaArtist = state.c_str();
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::muted_changed(std::string state) {
  ESP_LOGI(TAG, "muted_changed: %s to %s", this->entity_id_.c_str(),
           state.c_str());
  muted_ = strcmp(state.c_str(), "on") == 0;
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::shuffle_changed(std::string state) {
  ESP_LOGI(TAG, "shuffle_changed: %s to %s", this->entity_id_.c_str(),
           state.c_str());
  shuffle_ = strcmp(state.c_str(), "on") == 0;
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::repeat_changed(std::string state) {
  ESP_LOGI(TAG, "repeat_changed: %s to %s", this->entity_id_.c_str(),
           state.c_str());
  if (strcmp(state.c_str(), "all") == 0) {
    repeat_mode_ = ALL;
  } else if (strcmp(state.c_str(), "one") == 0) {
    repeat_mode_ = ONE;
  } else if (strcmp(state.c_str(), "off") == 0) {
    repeat_mode_ = OFF;
  } else {
    repeat_mode_ = NOT_SET;
  }
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::volume_changed(std::string state) {
  ESP_LOGI(TAG, "%s volume_changed to %s", this->entity_id_.c_str(),
           state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  volume = parse_number<float>(state).value_or(-1.0f);
}

void HomeAssistantBaseMediaPlayer::media_duration_changed(std::string state) {
  ESP_LOGI(TAG, "%s media_duration_changed to %s", this->entity_id_.c_str(),
           state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  mediaDuration = atof(state.c_str());
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::media_position_changed(std::string state) {
  ESP_LOGD(TAG, "%s media_position_changed to %s", this->entity_id_.c_str(),
           state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  mediaPosition = atof(state.c_str());
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::playlist_changed(std::string state) {
  ESP_LOGI(TAG, "%s playlist_changed to %s", this->entity_id_.c_str(),
           state.c_str());
  if (!can_update_from_api()) {
    return;
  }
  playlist_title = state.c_str();
  this->publish_state();
}

void HomeAssistantBaseMediaPlayer::tokenize(std::string const& str,
                                            std::string delim,
                                            std::vector<std::string>* out) {
  size_t start;
  size_t end = 0;

  while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
    end = str.find(delim, start);
    out->push_back(str.substr(start, end - start));
  }
}

std::string HomeAssistantBaseMediaPlayer::filter(std::string str) {
  std::string output;
  output.reserve(
      str.size());  // optional, avoids buffer reallocations in the loop
  for (size_t i = 0; i < str.size(); ++i) {
    if (i == 0 && str[i] == ' ')
      continue;
    if (i < str.size() - 3 && str[i] == '\\' && str[i + 1] == 'x' &&
        str[i + 2] == 'a') {
      // replace \xa with space
      output += ' ';
      i += 3;
      continue;
    }
    if (i == str.size() - 1 && str[i] == '}')
      return output;
    if (str[i] != '[' && str[i] != ']' && str[i] != '\'' && str[i] != '"')
      output += str[i];
  }
  return output;
}

void HomeAssistantBaseMediaPlayer::group_members_changed(std::string state) {
  ESP_LOGI(TAG, "group_members_changed: %s changed to %s",
           this->entity_id_.c_str(), state.c_str());
  if (!can_update_from_api()) {
    ESP_LOGW(TAG, "group_members_changed: cant update");
    return;
  }
  groupMembers.clear();
  std::vector<std::string>* out = new std::vector<std::string>();
  tokenize(state, ",", out);
  for (auto state = out->begin(); state != out->end(); ++state) {
    std::string newGroupedSpeaker = filter(*state);
    groupMembers.push_back(newGroupedSpeaker);
  }
  this->publish_state();
}

bool HomeAssistantBaseMediaPlayer::supports(
    MediaPlayerSupportedFeature feature) {
  for (auto iter = supported_features_.begin();
       iter != supported_features_.end(); ++iter) {
    if (feature == *(iter->get())) {
      return true;
    }
  }
  return false;
}

void HomeAssistantBaseMediaPlayer::toggle_shuffle() {
  ESP_LOGI(TAG, "toggle_shuffle: %s", this->entity_id_.c_str());
  call_homeassistant_service("media_player.shuffle_set",
                             {
                                 {"entity_id", this->entity_id_},
                                 {"shuffle", is_shuffling() ? "false" : "true"},
                             });
}

void HomeAssistantBaseMediaPlayer::toggle_mute() {
  ESP_LOGI(TAG, "toggle_mute: %s", this->entity_id_.c_str());
  call_homeassistant_service(
      "media_player.volume_mute",
      {
          {"entity_id", this->entity_id_},
          {"is_volume_muted", is_muted() ? "false" : "true"},
      });
}

void HomeAssistantBaseMediaPlayer::toggle_repeat() {
  std::string repeat_title;
  if (repeat_mode_ == NOT_SET) {
    ESP_LOGI(TAG, "toggle_repeat: %s not set", this->entity_id_.c_str());
    return;
  } else {
    switch (repeat_mode_) {
      case OFF:
        repeat_mode_ = ALL;
        repeat_title = "all";
        break;
      case ALL:
        repeat_mode_ = ONE;
        repeat_title = "one";
        break;
      case ONE:
        repeat_mode_ = OFF;
        repeat_title = "off";
        break;
    }
  }
  ESP_LOGI(TAG, "toggle_repeat: %s - %s", this->entity_id_.c_str(),
           repeat_title.c_str());
  call_homeassistant_service("media_player.repeat_set",
                             {
                                 {"entity_id", this->entity_id_},
                                 {"repeat", repeat_title.c_str()},
                             });
}

void HomeAssistantBaseMediaPlayer::increaseVolume() {
  // if (speaker_volume == -1) {
  //   localVolume = 0;
  //   return;
  // }
  // if (localVolume + volume_step_ > 1) {
  //   localVolume = 1;
  // } else {
  //   localVolume = localVolume + volume_step_;
  // }
  volume = std::min(1.0f, volume + volume_step_);
  updateVolumeLevel();
}

void HomeAssistantBaseMediaPlayer::decreaseVolume() {
  // if (speaker_volume == -1 || localVolume - volume_step_ < 0) {
  //   localVolume = 0;
  //   return;
  // }
  // if (localVolume - volume_step_ > 1) {
  //   localVolume = 0;
  // } else {
  //   localVolume = localVolume - volume_step_;
  // }
  volume = std::max(0.0f, volume - volume_step_);
  updateVolumeLevel();
}

void HomeAssistantBaseMediaPlayer::updateVolumeLevel() {
  ignore_api_updates_with_seconds(2);
  std::string entityIds = this->entity_id_;
  if (is_muted()) {
    // unmute all speakers
    call_homeassistant_service("media_player.volume_mute",
                               {
                                   {"entity_id", this->entity_id_.c_str()},
                                   {"is_volume_muted", "false"},
                               });
  }
  ESP_LOGI(TAG, "%s volume update %f", this->entity_id_.c_str(), volume);
  call_homeassistant_service("media_player.volume_set",
                             {
                                 {"entity_id", this->entity_id_.c_str()},
                                 {"volume_level", to_string(volume)},
                             });
}
}  // namespace homeassistant_media_player
}  // namespace esphome
