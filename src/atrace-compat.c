/*
 * Copyright (C) 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "atrace-compat.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "percetto.h"


struct atrace_group_tracker {
  uint64_t tags[PERCETTO_MAX_GROUP_CATEGORIES];
  struct percetto_category categories[PERCETTO_MAX_GROUP_CATEGORIES];
  int count;
};

struct atrace_counter_tracker {
  struct percetto_track tracks[PERCETTO_MAX_TRACKS];
  int count;
};

enum percetto_status {
  PERCETTO_STATUS_NOT_STARTED = 0,
  PERCETTO_STATUS_OK,
  PERCETTO_STATUS_ERROR,
};

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static volatile enum percetto_status s_percetto_status =
    PERCETTO_STATUS_NOT_STARTED;

static struct atrace_group_tracker s_groups;

static struct atrace_counter_tracker s_tracks;

// This category is never registered so it remains disabled permanently.
PERCETTO_CATEGORY_DEFINE(never, "", 0);

PERCETTO_CATEGORY_DEFINE(always, "ATRACE_TAG_ALWAYS", 0);
PERCETTO_CATEGORY_DEFINE(gfx, "Graphics", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(input, "Input", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(view, "View System", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(webview, "WebView", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(wm, "Window Manager", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(am, "Activity Manager", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(sm, "Sync Manager", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(audio, "Audio", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(video, "Video", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(camera, "Camera", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(hal, "Hardware Modules", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(app, "ATRACE_TAG_APP", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(res, "ATRACE_TAG_VIEW", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(dalvik, "Dalvik VM", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(rs, "RenderScript", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(bionic, "Bionic C Library", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(power, "Power Management", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(pm, "Package Manager", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(ss, "System Server", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(database, "Database", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(network, "Network", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(adb, "ADB", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(vibrator, "Vibrator", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(aidl, "AIDL calls", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(nnapi, "NNAPI", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(rro, "ATRACE_TAG_RRO", PERCETTO_CATEGORY_FLAG_SLOW);
PERCETTO_CATEGORY_DEFINE(sysprop, "ATRACE_TAG_SYSPROP", PERCETTO_CATEGORY_FLAG_SLOW);

// The order in this array exactly matches the ATRACE_TAG bit shifts:
static struct percetto_category* g_categories[] = {
  PERCETTO_CATEGORY_PTR(always), // ATRACE_TAG_ALWAYS        (1<<0)
  PERCETTO_CATEGORY_PTR(gfx), // ATRACE_TAG_GRAPHICS         (1<<1)
  PERCETTO_CATEGORY_PTR(input), // ATRACE_TAG_INPUT          (1<<2)
  PERCETTO_CATEGORY_PTR(view), // ATRACE_TAG_VIEW            (1<<3)
  PERCETTO_CATEGORY_PTR(webview), // ATRACE_TAG_WEBVIEW      (1<<4)
  PERCETTO_CATEGORY_PTR(wm), // ATRACE_TAG_WINDOW_MANAGER    (1<<5)
  PERCETTO_CATEGORY_PTR(am), // ATRACE_TAG_ACTIVITY_MANAGER  (1<<6)
  PERCETTO_CATEGORY_PTR(sm), // ATRACE_TAG_SYNC_MANAGER      (1<<7)
  PERCETTO_CATEGORY_PTR(audio), // ATRACE_TAG_AUDIO          (1<<8)
  PERCETTO_CATEGORY_PTR(video), // ATRACE_TAG_VIDEO          (1<<9)
  PERCETTO_CATEGORY_PTR(camera), // ATRACE_TAG_CAMERA        (1<<10)
  PERCETTO_CATEGORY_PTR(hal), // ATRACE_TAG_HAL              (1<<11)
  PERCETTO_CATEGORY_PTR(app), // ATRACE_TAG_APP              (1<<12)
  PERCETTO_CATEGORY_PTR(res), // ATRACE_TAG_RESOURCES        (1<<13)
  PERCETTO_CATEGORY_PTR(dalvik), // ATRACE_TAG_DALVIK        (1<<14)
  PERCETTO_CATEGORY_PTR(rs), // ATRACE_TAG_RS                (1<<15)
  PERCETTO_CATEGORY_PTR(bionic), // ATRACE_TAG_BIONIC        (1<<16)
  PERCETTO_CATEGORY_PTR(power), // ATRACE_TAG_POWER          (1<<17)
  PERCETTO_CATEGORY_PTR(pm), // ATRACE_TAG_PACKAGE_MANAGER   (1<<18)
  PERCETTO_CATEGORY_PTR(ss), // ATRACE_TAG_SYSTEM_SERVER     (1<<19)
  PERCETTO_CATEGORY_PTR(database), // ATRACE_TAG_DATABASE    (1<<20)
  PERCETTO_CATEGORY_PTR(network), // ATRACE_TAG_NETWORK      (1<<21)
  PERCETTO_CATEGORY_PTR(adb), // ATRACE_TAG_ADB              (1<<22)
  PERCETTO_CATEGORY_PTR(vibrator), // ATRACE_TAG_VIBRATOR    (1<<23)
  PERCETTO_CATEGORY_PTR(aidl), // ATRACE_TAG_AIDL            (1<<24)
  PERCETTO_CATEGORY_PTR(nnapi), // ATRACE_TAG_NNAPI          (1<<25)
  PERCETTO_CATEGORY_PTR(rro), // ATRACE_TAG_RRO              (1<<26)
  PERCETTO_CATEGORY_PTR(sysprop), // ATRACE_TAG_SYSPROP       (1<<27)
};

#define SCOPED_LOCK(mutex) \
   pthread_mutex_lock(&(mutex)); \
   pthread_mutex_t* PERCETTO_UID(unlocker) \
      __attribute__((cleanup(scoped_unlock), unused)) = &(mutex)

static inline void scoped_unlock(pthread_mutex_t** mutex) {
  pthread_mutex_unlock(*mutex);
}

void atrace_init() {
  SCOPED_LOCK(s_lock);
  if (s_percetto_status != PERCETTO_STATUS_NOT_STARTED)
    return;

  int ret = percetto_init(sizeof(g_categories) / sizeof(g_categories[0]),
                          g_categories, PERCETTO_CLOCK_DONT_CARE);
  if (ret < 0) {
    fprintf(stderr, "error: percetto_init failed: %d\n", ret);
    s_percetto_status = PERCETTO_STATUS_ERROR;
  } else {
    s_percetto_status = PERCETTO_STATUS_OK;
  }
}

struct percetto_category* atrace_create_category(uint64_t tags) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  if (s_percetto_status != PERCETTO_STATUS_OK ||
      !(tags & ATRACE_TAG_VALID_MASK))
    return PERCETTO_CATEGORY_PTR(never);

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_groups.count; ++i) {
    if (s_groups.tags[i] == tags)
      return &s_groups.categories[i];
  }

  // Create the array of category indices for this group.
  uint32_t static_count = sizeof(g_categories) / sizeof(g_categories[0]);
  assert(static_count <= 256);
  struct percetto_category_group group = { .child_ids = { 0 }, .count = 0 };
  for (uint32_t i = 0; i < static_count; ++i) {
    if (!!(tags & (1 << i))) {
      if (group.count == PERCETTO_MAX_GROUP_SIZE) {
        fprintf(stderr, "%s warning: too many categories in group\n",
                __func__);
        break;
      }
      group.child_ids[group.count++] = (uint8_t)i;
    }
  }

  if (group.count == 0) {
    // No categories were specified, so these trace events should never
    // trigger.
    return PERCETTO_CATEGORY_PTR(never);
  }

  if (group.count == 1) {
    // One category specified so return its pointer.
    uint32_t index = group.child_ids[0];
    assert(index < static_count);
    return g_categories[index];
  }

  if (s_groups.count == PERCETTO_MAX_GROUP_CATEGORIES) {
    fprintf(stderr, "%s error: too many different atrace combinations used\n",
            __func__);
    return PERCETTO_CATEGORY_PTR(never);
  }

  // Allocate and return a group category.
  struct percetto_category* category = &s_groups.categories[s_groups.count];
  s_groups.categories[s_groups.count] =
      (struct percetto_category)PERCETTO_CATEGORY_GROUP(group);
  s_groups.tags[s_groups.count] = tags;
  ++s_groups.count;

  // Holding lock during register to avoid another thread using the same
  // category before it's ready.
  percetto_register_group_category(category);

  return category;
}

uint64_t atrace_create_counter(const char* name) {
  if (s_percetto_status == PERCETTO_STATUS_NOT_STARTED)
    atrace_init();

  SCOPED_LOCK(s_lock);

  if (s_percetto_status != PERCETTO_STATUS_OK)
    return 0;

  // Check if we already have a matching group category for these tags.
  for (int i = 0; i < s_tracks.count; ++i) {
    if (s_tracks.tracks[i].name == name)
      return s_tracks.tracks[i].uuid;
  }

  if (s_tracks.count == PERCETTO_MAX_TRACKS) {
    fprintf(stderr, "%s error: too many atrace counters used\n", __func__);
    return 0;
  }

  struct percetto_track* track = &s_tracks.tracks[s_tracks.count];
  ++s_tracks.count;
  track->name = name;
  track->uuid = s_tracks.count + 1337; // arbitrary uuid
  track->type = PERCETTO_TRACK_COUNTER;

  // Holding lock during register to avoid another thread using the same track
  // before it's ready.
  percetto_register_track(track);

  return track->uuid;
}
