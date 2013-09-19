// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <string>

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/common/encrypted_media_messages_android.h"
#endif

using content::KeySystemInfo;

static const char kAudioWebM[] = "audio/webm";
static const char kVideoWebM[] = "video/webm";
static const char kVorbis[] = "vorbis";
static const char kVorbisVP8[] = "vorbis,vp8,vp8.0";

static const char kAudioMp4[] = "audio/mp4";
static const char kVideoMp4[] = "video/mp4";
static const char kMp4a[] = "mp4a";
static const char kAvc1[] = "avc1";
static const char kMp4aAvc1[] = "mp4a,avc1";

#if defined(ENABLE_PEPPER_CDMS)
static bool IsPepperCdmRegistered(const std::string& pepper_type) {
  bool is_registered = false;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_IsInternalPluginRegisteredForMimeType(
          pepper_type, &is_registered));

  return is_registered;
}

// External Clear Key (used for testing).
static void AddExternalClearKey(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  static const char kExternalClearKeyPepperType[] =
      "application/x-ppapi-clearkey-cdm";

  if (!IsPepperCdmRegistered(kExternalClearKeyPepperType))
    return;

  KeySystemInfo info(kExternalClearKeyKeySystem);

  info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
  info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1));
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.pepper_type = kExternalClearKeyPepperType;

  concrete_key_systems->push_back(info);
}
#endif  // defined(ENABLE_PEPPER_CDMS)


#if defined(WIDEVINE_CDM_AVAILABLE)
enum WidevineCdmType {
  WIDEVINE,
  WIDEVINE_HR,
  WIDEVINE_HRSURFACE,
};

// Defines bitmask values used to specify supported codecs.
// Each value represents a codec within a specific container.
enum SupportedCodecs {
  WEBM_VP8_AND_VORBIS = 1 << 0,
#if defined(USE_PROPRIETARY_CODECS)
  MP4_AAC = 1 << 1,
  MP4_AVC1 = 1 << 2,
#endif  // defined(USE_PROPRIETARY_CODECS)
};

#if defined(OS_ANDROID)
#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(name) == \
                 static_cast<int>(android::name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(WEBM_VP8_AND_VORBIS);
COMPILE_ASSERT_MATCHING_ENUM(MP4_AAC);
COMPILE_ASSERT_MATCHING_ENUM(MP4_AVC1);
#undef COMPILE_ASSERT_MATCHING_ENUM

static const uint8 kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };
#else
static bool IsWidevineHrSupported() {
  // TODO(jrummell): Need to call CheckPlatformState() but it is
  // asynchronous, and needs to be done in the browser.
  return false;
}
#endif

// Return |name|'s parent key system.
static std::string GetDirectParentName(std::string name) {
  int last_period = name.find_last_of('.');
  DCHECK_GT(last_period, 0);
  return name.substr(0, last_period);
}

static void AddWidevineWithCodecs(
    WidevineCdmType widevine_cdm_type,
    SupportedCodecs supported_codecs,
    std::vector<KeySystemInfo>* concrete_key_systems) {

  KeySystemInfo info(kWidevineKeySystem);

  switch (widevine_cdm_type) {
    case WIDEVINE:
      // For standard Widevine, add parent name.
      info.parent_key_system = GetDirectParentName(kWidevineKeySystem);
      break;
    case WIDEVINE_HR:
      info.key_system.append(".hr");
      break;
    case WIDEVINE_HRSURFACE:
      info.key_system.append(".hrsurface");
      break;
    default:
      NOTREACHED();
  }

  if (supported_codecs & WEBM_VP8_AND_VORBIS) {
    info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
    info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
  }

#if defined(USE_PROPRIETARY_CODECS)
  if (supported_codecs & MP4_AAC)
    info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));

  if (supported_codecs & MP4_AVC1) {
    const char* video_codecs = (supported_codecs & MP4_AAC) ? kMp4aAvc1 : kAvc1;
    info.supported_types.push_back(std::make_pair(kVideoMp4, video_codecs));
  }
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(ENABLE_PEPPER_CDMS)
  info.pepper_type = kWidevineCdmPluginMimeType;
#elif defined(OS_ANDROID)
  info.uuid.assign(kWidevineUuid, kWidevineUuid + arraysize(kWidevineUuid));
#endif  // defined(ENABLE_PEPPER_CDMS)

  concrete_key_systems->push_back(info);
}

#if defined(ENABLE_PEPPER_CDMS)
// Supported types are determined at compile time.
static void AddPepperBasedWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  if (!IsPepperCdmRegistered(kWidevineCdmPluginMimeType)) {
    DVLOG(1) << "Widevine CDM is not currently available.";
    return;
  }

  SupportedCodecs supported_codecs = WEBM_VP8_AND_VORBIS;

#if defined(USE_PROPRIETARY_CODECS)
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
  supported_codecs = static_cast<SupportedCodecs>(supported_codecs | MP4_AAC);
#endif
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
  supported_codecs = static_cast<SupportedCodecs>(supported_codecs | MP4_AVC1);
#endif
#endif  // defined(USE_PROPRIETARY_CODECS)

  AddWidevineWithCodecs(WIDEVINE, supported_codecs, concrete_key_systems);

  if (IsWidevineHrSupported())
    AddWidevineWithCodecs(WIDEVINE_HR, supported_codecs, concrete_key_systems);
}
#elif defined(OS_ANDROID)
static void AddAndroidWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  android::SupportedKeySystemRequest request;
  android::SupportedKeySystemResponse response;

  request.uuid.insert(request.uuid.begin(), kWidevineUuid,
                      kWidevineUuid + arraysize(kWidevineUuid));
#if defined(USE_PROPRIETARY_CODECS)
  request.codecs = static_cast<android::SupportedCodecs>(
      android::MP4_AAC | android::MP4_AVC1);
#endif  // defined(USE_PROPRIETARY_CODECS)
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetSupportedKeySystems(request, &response));
  DCHECK_EQ(response.compositing_codecs >> 3, 0) << "unrecognized codec";
  DCHECK_EQ(response.non_compositing_codecs >> 3, 0) << "unrecognized codec";
  if (response.compositing_codecs > 0) {
    AddWidevineWithCodecs(
        WIDEVINE,
        static_cast<SupportedCodecs>(response.compositing_codecs),
        concrete_key_systems);
  }

  if (response.non_compositing_codecs > 0) {
    AddWidevineWithCodecs(
        WIDEVINE_HRSURFACE,
        static_cast<SupportedCodecs>(response.non_compositing_codecs),
        concrete_key_systems);
  }
}
#endif  // defined(ENABLE_PEPPER_CDMS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

void AddChromeKeySystems(std::vector<KeySystemInfo>* key_systems_info) {
#if defined(ENABLE_PEPPER_CDMS)
  AddExternalClearKey(key_systems_info);
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)
#if defined(ENABLE_PEPPER_CDMS)
  AddPepperBasedWidevine(key_systems_info);
#elif defined(OS_ANDROID)
  AddAndroidWidevine(key_systems_info);
#endif
#endif
}
