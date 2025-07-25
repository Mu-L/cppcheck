/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2025 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stacktrace.h"

#ifdef USE_UNIX_BACKTRACE_SUPPORT

#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <execinfo.h>

void print_stacktrace(FILE* output, int start_idx, bool demangling, int maxdepth, bool omit_above_own)
{
    // 32 vs. 64bit
#define ADDRESSDISPLAYLENGTH ((sizeof(long)==8)?12:8)
    void *callstackArray[32]= {nullptr}; // the less resources the better...
    const int currentdepth = backtrace(callstackArray, static_cast<int>(getArrayLength(callstackArray)));
    // set offset to 1 to omit the printing function itself
    int offset=start_idx+1; // some entries on top are within our own exception handling code or libc
    if (maxdepth<0)
        maxdepth=currentdepth-offset;
    else
        maxdepth = std::min(maxdepth, currentdepth);

    char **symbolStringList = backtrace_symbols(callstackArray, currentdepth);
    if (!symbolStringList) {
        fputs("Callstack could not be obtained\n", output);
        return;
    }

    fputs("Callstack:\n", output);
    bool own_code = false;
    char demangle_buffer[2048]= {0};
    for (int i = offset; i < maxdepth; ++i) {
        const char * const symbolString = symbolStringList[i];
        // skip all leading libc symbols so the first symbol is our code
        if (omit_above_own && !own_code) {
            if (strstr(symbolString, "/libc.so.6") != nullptr)
                continue;
            own_code = true;
            offset = i; // make sure the numbering is continous if we omit frames
        }
        char * realnameString = nullptr;
        const char * const firstBracketName     = strchr(symbolString, '(');
        const char * const firstBracketAddress  = strchr(symbolString, '[');
        const char * const secondBracketAddress = strchr(firstBracketAddress, ']');
        const char * const beginAddress         = firstBracketAddress+3;
        const int addressLen = int(secondBracketAddress-beginAddress);
        const int padLen     = (ADDRESSDISPLAYLENGTH-addressLen);
        if (demangling && firstBracketName) {
            const char * const plus = strchr(firstBracketName, '+');
            if (plus && (plus>(firstBracketName+1))) {
                char input_buffer[1024]= {0};
                strncpy(input_buffer, firstBracketName+1, plus-firstBracketName-1);
                size_t length = getArrayLength(demangle_buffer);
                int status=0;
                // We're violating the specification - passing stack address instead of malloc'ed heap.
                // Benefit is that no further heap is required, while there is sufficient stack...
                realnameString = abi::__cxa_demangle(input_buffer, demangle_buffer, &length, &status); // non-NULL on success
            }
        }
        const int ordinal=i-offset;
        fprintf(output, "#%-2d 0x",
                ordinal);
        if (padLen>0)
            fprintf(output, "%0*d",
                    padLen, 0);
        if (realnameString) {
            fprintf(output, "%.*s in %s\n",
                    static_cast<int>(secondBracketAddress - firstBracketAddress - 3), firstBracketAddress+3,
                    realnameString);
        } else {
            fprintf(output, "%.*s in %.*s\n",
                    static_cast<int>(secondBracketAddress - firstBracketAddress - 3), firstBracketAddress+3,
                    static_cast<int>(firstBracketAddress - symbolString), symbolString);
        }
    }
    // NOLINTNEXTLINE(bugprone-multi-level-implicit-pointer-conversion) - code matches the documented usage
    free(symbolStringList);
#undef ADDRESSDISPLAYLENGTH
}

#endif // USE_UNIX_BACKTRACE_SUPPORT
