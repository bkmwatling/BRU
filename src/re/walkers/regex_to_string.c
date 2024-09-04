#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../utils.h"
#include "regex_to_string.h"

/* --- Type definitions ----------------------------------------------------- */

#define SB_ENSURE_SPACE(sb, n) \
    BRU_ENSURE_SPACE((sb)->s, (sb)->len, (sb)->cap, sizeof(char))

#define DEFAULT_CAP 32

typedef struct {
    char  *s;
    size_t len;
    size_t cap;
} BruStringBuilder;

static void appendn(BruStringBuilder *sb, const char *s, size_t n)
{
    SB_ENSURE_SPACE(sb, n);
    strncpy(sb->s + sb->len, s, n);
    sb->len += n;
}

static void append(BruStringBuilder *sb, const char *s)
{
    appendn(sb, s, strlen(s));
}

static void appendf(BruStringBuilder *sb, const char *fmt, ...)
{
    char   *s;
    size_t  len;
    va_list args;

    va_start(args, fmt);
    // includes terminating '\0'
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    printf("append: %s\n", fmt);
    s = malloc(sizeof(char) * len);
    if (!s) return;

    va_start(args, fmt);
    vsnprintf(s, len, fmt, args);
    va_end(args);

    SB_ENSURE_SPACE(sb, len);
    strncpy(sb->s + sb->len, s, sb->cap - sb->len);
    sb->len += len - 1;

    free(s);
}

static BruStringBuilder *string_builder(void)
{
    BruStringBuilder *sb = malloc(sizeof(BruStringBuilder));

    if (!sb) return NULL;

    sb->s   = malloc(sizeof(char) * DEFAULT_CAP);
    sb->cap = DEFAULT_CAP;
    sb->len = 0;

    return sb;
}

/* --- Listener functions --------------------------------------------------- */

BRU_LISTEN_TERMINAL_F()
{
    BRU_GET_STATE(BruStringBuilder *, sb);
    switch (BRU_REGEX->type) {
        case BRU_CARET: append(sb, "^"); break;
        case BRU_DOLLAR: append(sb, "$"); break;
        case BRU_MEMOISE: append(sb, "#"); break;
        case BRU_LITERAL:
            appendn(sb, BRU_REGEX->ch, stc_utf8_nbytes(BRU_REGEX->ch));
            break;

        case BRU_CC: printf("CC printer not supported"); // TODO
        default: break;
    }
}

/* --- ALT ------------------------------------------------------------------ */

BRU_LISTENER_F(BRU_INORDER, BRU_ALT)
{
    BRU_GET_STATE(BruStringBuilder *, sb);
    append(sb, "|");
    if (BRU_REGEX->right->type == BRU_ALT) {
        // assoiativity was overridden -- insert (?:
        append(sb, "(?:");
    }
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_ALT)
{
    BRU_GET_STATE(BruStringBuilder *, sb);
    if (BRU_REGEX->right->type == BRU_ALT) {
        // assoiativity was overridden -- insert closing )
        append(sb, ")");
    }
}

/* --- CONCAT --------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_CONCAT)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // lower priority operators as child must have parenthesis
    if (BRU_REGEX->left->type == BRU_ALT) append(sb, "(?:");
}

BRU_LISTENER_F(BRU_INORDER, BRU_CONCAT)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // lower priority operators as child must have parenthesis
    if (BRU_REGEX->left->type == BRU_ALT) append(sb, ")");

    if (BRU_IS_BINARY_OP(BRU_REGEX->right->type)) {
        // ALT => as above
        // CONCAT => associativity was overriden
        append(sb, "(?:");
    }
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_CONCAT)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    if (BRU_IS_BINARY_OP(BRU_REGEX->right->type)) append(sb, ")");
}

/* --- CAPTURE -------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_CAPTURE)
{
    BRU_GET_STATE(BruStringBuilder *, sb);
    append(sb, "(");

    (void) BRU_REGEX;
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_CAPTURE)
{
    BRU_GET_STATE(BruStringBuilder *, sb);
    append(sb, ")");

    (void) BRU_REGEX;
}

/* --- STAR ----------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_STAR)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // not terminal or parenthetical
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, "(?:");
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_STAR)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // see above
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, ")");

    append(sb, "*");
    if (!BRU_REGEX->greedy) append(sb, "?"); // lazy
}

/* --- PLUS ----------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_PLUS)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // not terminal or parenthetical
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, "(?:");
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_PLUS)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // see above
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, ")");

    append(sb, "+");
    if (!BRU_REGEX->greedy) append(sb, "?"); // lazy
}

/* --- QUES ----------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_QUES)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // not terminal or parenthetical
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, "(?:");
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_QUES)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // see above
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, ")");

    append(sb, "?");
    if (!BRU_REGEX->greedy) append(sb, "?"); // lazy
}

/* --- COUNTER -------------------------------------------------------------- */

BRU_LISTENER_F(BRU_PREORDER, BRU_COUNTER)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // not terminal or parenthetical
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, "(?:");
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_COUNTER)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    // see above
    if (BRU_IS_OP(BRU_REGEX->left->type)) append(sb, ")");

    appendf(sb, "{" BRU_CNTR_FMT ", " BRU_CNTR_FMT "}", BRU_REGEX->min,
            BRU_REGEX->max);
    if (!BRU_REGEX->greedy) append(sb, "?"); // lazy
}

/* --- LOOKAHEAD ------------------------------------------------------------ */

BRU_LISTENER_F(BRU_PREORDER, BRU_LOOKAHEAD)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    append(sb, "(?");
    if (BRU_REGEX->positive)
        append(sb, "=");
    else
        append(sb, "!");
}

BRU_LISTENER_F(BRU_POSTORDER, BRU_LOOKAHEAD)
{
    BRU_GET_STATE(BruStringBuilder *, sb);

    append(sb, ")");
    (void) BRU_REGEX;
}

/* --- API function definitions --------------------------------------------- */

char *bru_regex_to_string(BruRegexNode *re)
{
    BruWalker        *w;
    BruStringBuilder *sb;
    char             *str;

    if (!re) return NULL;

    w = bru_walker_new();
    BRU_REGISTER_LISTEN_TERMINAL_F(w);
    BRU_REGISTER_LISTENER_F(w, BRU_INORDER, BRU_ALT);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_ALT);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_CONCAT);
    BRU_REGISTER_LISTENER_F(w, BRU_INORDER, BRU_CONCAT);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_CONCAT);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_CAPTURE);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_CAPTURE);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_STAR);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_STAR);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_PLUS);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_PLUS);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_QUES);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_QUES);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_COUNTER);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_COUNTER);

    BRU_REGISTER_LISTENER_F(w, BRU_PREORDER, BRU_LOOKAHEAD);
    BRU_REGISTER_LISTENER_F(w, BRU_POSTORDER, BRU_LOOKAHEAD);

    w->state = (void *) string_builder();
    bru_walker_walk(w, &re);

    sb = (BruStringBuilder *) w->state;
    SB_ENSURE_SPACE(sb, 1);
    sb->s[sb->len] = '\0';

    str = sb->s;

    // clean up
    bru_walker_free(w);
    free(sb);

    return str;
}
