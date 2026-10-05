//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// Compile-time strings must remain independent of per-commit build metadata.
// Commit-dependent values are defined out-of-line in src/product.cpp so they
// do not invalidate every translation unit that includes product.h.
#define CTS_BUILD_STR static inline constexpr StringRef
#define RTS_BUILD_STR static const StringRef

// simple class for bot internal information
static constexpr class Product final {
public:
   explicit constexpr Product () = default;
   ~Product () = default;

public:
   static constexpr struct BuildInfo {
      RTS_BUILD_STR hash;
      RTS_BUILD_STR count;
      RTS_BUILD_STR author;
      RTS_BUILD_STR machine;
      RTS_BUILD_STR compiler;
      RTS_BUILD_STR id;
   } bi {};

public:
   CTS_BUILD_STR name { "YaPB" };
   CTS_BUILD_STR nameLower { "yapb" };
   RTS_BUILD_STR year;
   CTS_BUILD_STR author { "YaPB Project" };
   CTS_BUILD_STR email { "yapb@jeefo.net" };
   CTS_BUILD_STR url { "https://yapb.jeefo.net/" };
   CTS_BUILD_STR download { "yapb.jeefo.net" };
   CTS_BUILD_STR upload { "yapb.jeefo.net/upload" };
   CTS_BUILD_STR httpScheme { "http" };
   CTS_BUILD_STR logtag { "YB" };
   RTS_BUILD_STR dtime;
   RTS_BUILD_STR date;
   RTS_BUILD_STR version;
   CTS_BUILD_STR cmdPri { "yb" };
   CTS_BUILD_STR cmdSec { "yapb" };
} product {};

static constexpr class Folders final {
public:
   explicit constexpr Folders () = default;
   ~Folders () = default;

public:
   CTS_BUILD_STR bot { product.nameLower };
   CTS_BUILD_STR addons { "addons" };
   CTS_BUILD_STR config { "conf" };
   CTS_BUILD_STR data { "data" };
   CTS_BUILD_STR lang { "lang" };
   CTS_BUILD_STR logs { "logs" };
   CTS_BUILD_STR train { "train" };
   CTS_BUILD_STR graph { "graph" };
   CTS_BUILD_STR podbot { "pwf" };
   CTS_BUILD_STR bin { "bin" };
} folders {};

#undef RTS_BUILD_STR
#undef CTS_BUILD_STR
