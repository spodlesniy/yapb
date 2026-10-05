//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <crlib/crlib.h>

using namespace cr;

#include <product.h>

#ifdef VERSION_GENERATED
#  include <version.build.h>
#else
#  include <version.h>
#endif

const StringRef Product::BuildInfo::hash { MODULE_COMMIT_HASH };
const StringRef Product::BuildInfo::count { MODULE_COMMIT_COUNT };
const StringRef Product::BuildInfo::author { MODULE_AUTHOR };
const StringRef Product::BuildInfo::machine { MODULE_MACHINE };
const StringRef Product::BuildInfo::compiler { MODULE_COMPILER };
const StringRef Product::BuildInfo::id { MODULE_BUILD_ID };

const StringRef Product::year { MODULE_BUILD_YEAR };
const StringRef Product::dtime { MODULE_BUILD_DATETIME };
const StringRef Product::date { MODULE_BUILD_DATE };
const StringRef Product::version { MODULE_VERSION "." MODULE_COMMIT_COUNT };
