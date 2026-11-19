#include "types.h"
#include "sounddef.h"
#include "v2init.h"

int v2version = 0;
int *v2vsizes = nullptr;
int *v2gsizes = nullptr;

void v2_init_version_tables() {
    if (v2vsizes)
        return;

    v2version = 0;
    for (int i = 0; i < v2nparms; ++i)
        if (v2parms[i].version > v2version)
            v2version = v2parms[i].version;
    for (int i = 0; i < v2ngparms; ++i)
        if (v2gparms[i].version > v2version)
            v2version = v2gparms[i].version;

    v2vsizes = new int[v2version + 1]();
    v2gsizes = new int[v2version + 1]();

    for (int i = 0; i < v2nparms; ++i)
        for (int j = v2version; j >= v2parms[i].version; --j)
            ++v2vsizes[j];
    for (int i = 0; i < v2ngparms; ++i)
        for (int j = v2version; j >= v2gparms[i].version; --j)
            ++v2gsizes[j];

    // Each patch ends with one modulation count byte followed by 255
    // modulation triples; the converter relies on this trailer in the size.
    for (int i = 0; i <= v2version; ++i)
        v2vsizes[i] += 1 + 255 * 3;
}
