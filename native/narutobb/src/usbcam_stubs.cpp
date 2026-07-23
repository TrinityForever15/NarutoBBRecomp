// ReXGlue v0.8.0 ships xboxkrnl_usbcam.cpp in source, but the official
// Windows runtime library was built without it. Keep the upstream behavior
// locally so the Broken Bond's camera imports link and register correctly.
#include <rex/hook.h>
#include <rex/kernel/xboxkrnl/private.h>
#include <rex/system/xtypes.h>
#include <rex/types.h>

namespace rex::kernel::xboxkrnl {

u32 XUsbcamCreate_entry(u32 buffer, u32 buffer_size, mapped_void unk3_ptr) {
  return X_STATUS_SUCCESS;
}

u32 XUsbcamGetState_entry() {
  return 0;
}

}  // namespace rex::kernel::xboxkrnl

REX_EXPORT(__imp__XUsbcamCreate, rex::kernel::xboxkrnl::XUsbcamCreate_entry)
REX_EXPORT(__imp__XUsbcamGetState, rex::kernel::xboxkrnl::XUsbcamGetState_entry)

REX_EXPORT_STUB(__imp__XUsbcamSetCaptureMode);
REX_EXPORT_STUB(__imp__XUsbcamGetConfig);
REX_EXPORT_STUB(__imp__XUsbcamSetConfig);
REX_EXPORT_STUB(__imp__XUsbcamReadFrame);
REX_EXPORT_STUB(__imp__XUsbcamSnapshot);
REX_EXPORT_STUB(__imp__XUsbcamSetView);
REX_EXPORT_STUB(__imp__XUsbcamGetView);
REX_EXPORT_STUB(__imp__XUsbcamDestroy);
REX_EXPORT_STUB(__imp__XUsbcamReset);
