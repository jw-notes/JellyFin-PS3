#pragma once
// Copyright (C) 2026 jw-notes.  GPLv3, see LICENSE.
#include <stdint.h>

// -------------------------------------------------------------------------
//  A file on the server, read like a file on a drive
// -------------------------------------------------------------------------
//  Direct play reads the original file with HTTP range requests.  It reaches
//  the player's local-file path as the lfs path HTTP_FILE_PATH (lfs_set_net),
//  so the Matroska and transport stream readers need no second way in.  Plain
//  http:// only, for the same reason the video socket is (net/https_client.h).
//
//  Each handle keeps one connection and reads on along it.  A read elsewhere
//  drains a short way forward, and otherwise connects again at the new offset.
//  A connection that fails is retried; a read that still cannot finish fails,
//  which ends playback the way a pulled drive does.

#define HTTP_FILE_PATH "jfnet:/media"

// The file the next opens read.  False for a URL this cannot read.
bool http_file_set_url(const char *url);
