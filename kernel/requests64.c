/*
 * LINDOWS Phase 3 — Limine request markers for x86_64
 *
 * This file defines the .limine_requests section that the Limine bootloader
 * scans to discover what services the kernel requests and what base revision
 * it conforms to.  The section layout follows the Limine v12.10.0 specification:
 *
 *   .limine_requests_start  — start marker (4×64-bit values)
 *   .limine_requests        — array of request structs (if any)
 *   .limine_requests_end    — end marker (2×64-bit values)
 *
 * The Limine loader uses these markers to locate the requests region even
 * if the kernel ELF is relocated or stripped.  The base revision must match
 * the loader's SUPPORTED_BASE_REVISION (6 for Limine v12.10.0).
 *
 * References:
 *   - Limine protocol: tools/limine/protocol/limine.h
 *   - Template: tools/limine/template_main.c / template_x86_64.lds
 */

#include "types.h"

/* --- marker values (from Limine protocol, commit 6e85ddd) ---

 * Start marker: 4×64-bit values that the loader searches for at the
 *               beginning of the .limine_requests section.
 *   {0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, 0x785c6ed015d3e316,
 *    0x181e920a7852b9d9}
 *
 * End marker: 2×64-bit values at the end of the region.
 *   {0xadc0e0531bb10d03, 0x9572709f31764c62}
 *
 * These are defined in tools/limine/protocol/limine.h and also appear
 * in the template as LIMINE_REQUESTS_START_MARKER and
 * LIMINE_REQUESTS_END_MARKER.
 */

/* Start marker — 4 consecutive 64-bit values */
__attribute__((used, section(".limine_requests_start")))
static volatile u64 limine_requests_start_marker[4] = {
    0xf6b8f4b39de7d1ae,
    0xfab91a6940fcb9cf,
    0x785c6ed015d3e316,
    0x181e920a7852b9d9
};

/* End marker — 2 consecutive 64-bit values */
__attribute__((used, section(".limine_requests_end")))
static volatile u64 limine_requests_end_marker[2] = {
    0xadc0e0531bb10d03,
    0x9572709f31764c62
};

/* --- base revision 6 ---

 * The Limine bootloader requires (or at least recommends) base revision 6
 * for v12.10.0.  The macro LIMINE_BASE_REVISION(N) generates a 2×64-bit
 * array {0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N)}.  The third
 * element is the revision number; the loader checks that SUPPORTED_BASE_REVISION
 * matches, which for v12.10.0 means the third dword must be 6.
 *
 * Note: the loader's SUPPORTED_BASE_REVISION is 6, so we must use
 * LIMINE_BASE_REVISION(6).  Using a different revision (e.g. 0 or 5) will
 * cause the loader to reject the kernel or fall back to default behaviour.
 */
__attribute__((used, section(".limine_requests")))
static volatile u64 limine_base_revision[2] = LIMINE_BASE_REVISION(6);

/* --- optional: executable file request ---

 * If we wanted the loader to pass us the loaded ELF's file handle, we could
 * request it via LIMINE_EXECUTABLE_FILE_REQUEST_ID.  For Phase 3 milestone 1
 * we only need the base revision + markers, so this is commented out.
 * Uncomment and add the request if/when we need the loaded binary's details.
 */

/*
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_file_request
    limine_executable_file_request = {
        .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    };
*/

/* --- optional: entry point request ---

 * Similarly, LIMINE_ENTRY_POINT_REQUEST_ID could be used to have the loader
 * give us the entry point physical address.  Not needed for milestone 1.
 * Uncomment if needed.
 *
__attribute__((used, section(".limine_requests")))
static volatile struct limine_entry_point_request
    limine_entry_point_request = {
        .id = LIMINE_ENTRY_POINT_REQUEST_ID,
    };
*/