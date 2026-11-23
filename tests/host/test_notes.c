/* NOTES: the list in name order, the titles and the body. */
#include "rc_test.h"

#include <stdio.h>
#include <string.h>

#include "model/notes.h"

static dx_notes_t n;

static void offer(const char *name, uint32_t size, int dir)
{
    dx_dirent_t e;

    memset(&e, 0, sizeof(e));
    snprintf(e.name, sizeof(e.name), "%s", name);
    e.size = size;
    e.is_dir = (uint8_t)dir;
    dx_notes_offer(&n, &e);
}

static void title(int i, const char *text)
{
    dx_notes_title(&n, i, (const uint8_t *)text, (int32_t)strlen(text));
}

TEST(name_order_and_filter)
{
    dx_notes_begin(&n);
    offer("WALK02.TXT", 10, 0);
    offer("WALK01.TXT", 10, 0);
    offer("A.TXT", 10, 0);
    offer("LIST.DAT", 10, 0);    /* not .TXT */
    offer("walk.txt", 10, 0);    /* not upper case */
    offer("LONGNA~1.TXT", 10, 0); /* not 8.3 by the content rules */
    offer("SUB.TXT", 0, 1);      /* a folder */
    offer("A.TXT", 10, 0);       /* twice */
    offer("_1.TXT", 10, 0);
    CHECK_EQ(n.count, 4);
    CHECK_STR(n.notes[0].file, "A.TXT");
    CHECK_STR(n.notes[1].file, "WALK01.TXT");
    CHECK_STR(n.notes[2].file, "WALK02.TXT");
    CHECK_STR(n.notes[3].file, "_1.TXT"); /* byte order: '_' after 'W' */
}

TEST(first_32_in_name_order)
{
    char name[16];
    int i;

    dx_notes_begin(&n);
    /* 40 notes offered from the last to the first. */
    for (i = 39; i >= 0; i--) {
        snprintf(name, sizeof(name), "N%02d.TXT", i);
        offer(name, 5, 0);
    }
    CHECK_EQ(n.count, 32);
    CHECK_STR(n.notes[0].file, "N00.TXT");
    CHECK_STR(n.notes[31].file, "N31.TXT");
    /* And in mixed order. */
    dx_notes_begin(&n);
    for (i = 0; i < 40; i++) {
        snprintf(name, sizeof(name), "N%02d.TXT", (i * 17) % 40);
        offer(name, 5, 0);
    }
    CHECK_EQ(n.count, 32);
    for (i = 0; i < 32; i++) {
        snprintf(name, sizeof(name), "N%02d.TXT", i);
        CHECK_STR(n.notes[i].file, name);
    }
}

TEST(titles)
{
    dx_notes_begin(&n);
    offer("A.TXT", 0, 0);
    offer("B.TXT", 0, 0);
    offer("C.TXT", 0, 0);
    offer("D.TXT", 0, 0);
    offer("E.TXT", 0, 0);
    offer("F.TXT", 0, 0);
    offer("G.TXT", 0, 0);
    title(0, "Walk to the spring\r\nbody\r\n");
    title(1, "A title that runs past thirty!!\nx\n");
    title(2, "\nbody\n");      /* empty title: not shown */
    title(3, "  \t \nbody\n"); /* only spaces: not shown */
    dx_notes_title(&n, 4, NULL, DX_FILE_TOO_LARGE);
    title(5, "Only a title");
    /* G.TXT is never read: not shown. */
    dx_notes_finish(&n);
    CHECK_EQ(n.count, 3);
    CHECK_STR(n.notes[0].title, "Walk to the spring");
    CHECK_STR(n.notes[1].title, "A title that runs past thirty!");
    CHECK_STR(n.notes[2].file, "F.TXT");
    CHECK_EQ(n.notes[2].size, 12);
}

TEST(title_size_limit)
{
    static uint8_t big[DX_NOTE_FILE_MAX + 1];

    memset(big, 'x', sizeof(big));
    dx_notes_begin(&n);
    offer("A.TXT", 0, 0);
    offer("B.TXT", 0, 0);
    dx_notes_title(&n, 0, big, DX_NOTE_FILE_MAX + 1);
    dx_notes_title(&n, 1, big, DX_NOTE_FILE_MAX);
    dx_notes_finish(&n);
    CHECK_EQ(n.count, 1);
    CHECK_STR(n.notes[0].file, "B.TXT");
}

TEST(body)
{
    static const char text[] = "Title\r\nLine one\r\nTab\there\x01 nul\0end\n";
    char out[64];
    char small[6];

    CHECK_EQ(dx_notes_body((const uint8_t *)text, sizeof(text) - 1, out, sizeof(out)), 27);
    CHECK_STR(out, "Line one\nTab\there? nul?end\n");
    CHECK_EQ(dx_notes_body((const uint8_t *)text, sizeof(text) - 1, small, sizeof(small)), 5);
    CHECK_STR(small, "Line ");
    CHECK_EQ(dx_notes_body((const uint8_t *)"Title only", 10, out, sizeof(out)), 0);
    CHECK_STR(out, "");
    /* A lone CR stays a control byte. */
    CHECK_EQ(dx_notes_body((const uint8_t *)"T\na\rb", 5, out, sizeof(out)), 3);
    CHECK_STR(out, "a?b");
}
