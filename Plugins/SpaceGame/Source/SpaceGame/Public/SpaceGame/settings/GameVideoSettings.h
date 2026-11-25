#pragma once

#include <ioj/settings/player_settings.h>
#include <ioj/settings/video_settings.h>

namespace ioj {

class USpaceGameUserSettings;

class SPACEGAME_API FGameVideoSettings {
  public:
    auto apply_quality(VideoSettings const& state) const -> SettingsResult;

  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    void write_non_display(USpaceGameUserSettings& settings, VideoSettings const& state) const;
};

} // namespace ioj
