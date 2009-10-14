#ifndef ANTARES_STUB_RESOURCES_H_
#define ANTARES_STUB_RESOURCES_H_

#include <Base.h>

#include "Handle.hpp"

namespace antares {

typedef void* ResFile;

STUB0(CurResFile, int(), 0);
STUB1(CloseResFile, void(int res_file));
STUB1(UseResFile, void(int res_file));
STUB1(HomeResFile, int(Handle handle), 0);

STUB1(ReleaseResource, void(Handle handle));
STUB1(DetachResource, void(Handle handle));

enum {
    resNotFound = 2000,
};

}  // namespace antares

#endif // ANTARES_STUB_RESOURCES_H_
