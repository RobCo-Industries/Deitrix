/* The INV tab's state, for the host tests. */
#ifndef DX_SCREENS_INV_SCREEN_H
#define DX_SCREENS_INV_SCREEN_H

/* The layout of the body, from its top. */
#define DX_INV_LIST_DY    22 /* the list, under the sub-tabs */
#define DX_INV_LIST_ROWS  9
#define DX_INV_RULE_DY    169 /* a faint rule over the summary */
#define DX_INV_SUMMARY_DY 173
#define DX_INV_KV_DY      24 /* the detail's rows, under its title */
#define DX_INV_NOTE_GAP   8  /* between the rows and the note */

/* Back to the first category, row 0 everywhere, no detail open. The
 * ROM never calls it: the state lives as long as the ROM runs. */
void dx_inv_screen_reset(void);

#endif
