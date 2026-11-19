#pragma once

// Initialises the version tables required by v2mconv.cpp. Safe to call more
// than once; the tables live for the process lifetime.
void v2_init_version_tables();
