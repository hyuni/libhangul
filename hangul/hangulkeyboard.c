/* libhangul
 * Copyright (C) 2016 Choe Hwanjin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <locale.h>
#include <glob.h>
#include <libgen.h>
#include <expat.h>

#include "hangul-gettext.h"
#include "hangul.h"
#include "hangulinternals.h"


#define LIBHANGUL_KEYBOARD_DIR LIBHANGUL_DATA_DIR "/keyboards"
//#define LIBHANGUL_KEYBOARD_DIR TOP_SRCDIR "/data/keyboards"

#define HANGUL_KEYBOARD_TABLE_SIZE 0x80

typedef struct _HangulCombinationItem HangulCombinationItem;

struct _HangulCombinationItem {
    uint32_t key;
    ucschar code;
};

struct _HangulCombination {
    size_t size;
    size_t size_alloced;
    HangulCombinationItem *table;

    bool is_static;
};

struct _HangulKeyboard {
    char* id;
    char* name;
    ucschar* table[4];
    HangulCombination* combination[4];

    int type;
    bool is_static;
};

typedef struct _HangulKeyboardList {
    size_t n;
    size_t nalloced;
    HangulKeyboard** keyboards;
} HangulKeyboardList;

#include "hangulkeyboard.h"

static const HangulCombination hangul_combination_default = {
    countof(hangul_combination_table_default),
    countof(hangul_combination_table_default),
    (HangulCombinationItem*)hangul_combination_table_default,
    true
};

static const HangulCombination hangul_combination_romaja = {
    countof(hangul_combination_table_romaja),
    countof(hangul_combination_table_romaja),
    (HangulCombinationItem*)hangul_combination_table_romaja,
    true
};

static const HangulCombination hangul_combination_full = {
    countof(hangul_combination_table_full),
    countof(hangul_combination_table_full),
    (HangulCombinationItem*)hangul_combination_table_full,
    true
};

static const HangulCombination hangul_combination_ahn = {
    countof(hangul_combination_table_ahn),
    countof(hangul_combination_table_ahn),
    (HangulCombinationItem*)hangul_combination_table_ahn,
    true
};

static const HangulKeyboard hangul_keyboard_2 = {
    (char*)"2",
    (char*)N_("Dubeolsik"),
    { (ucschar*)hangul_keyboard_table_2, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_default, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JAMO,
    true
};

static const HangulKeyboard hangul_keyboard_2y = {
    (char*)"2y",
    (char*)N_("Dubeolsik Yetgeul"),
    { (ucschar*)hangul_keyboard_table_2y, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_full, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JAMO_YET,
    true
};

static const HangulKeyboard hangul_keyboard_32 = {
    (char*)"32",
    (char*)N_("Sebeolsik Dubeol Layout"),
    { (ucschar*)hangul_keyboard_table_32, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_default, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO,
    true
};

static const HangulKeyboard hangul_keyboard_390 = {
    (char*)"39",
    (char*)N_("Sebeolsik 390"),
    { (ucschar*)hangul_keyboard_table_390, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_default, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO,
    true
};

static const HangulKeyboard hangul_keyboard_3final = {
    (char*)"3f",
    (char*)N_("Sebeolsik Final"),
    { (ucschar*)hangul_keyboard_table_3final, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_default, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO,
    true
};

static const HangulKeyboard hangul_keyboard_3sun = {
    (char*)"3s",
    (char*)N_("Sebeolsik Noshift"),
    { (ucschar*)hangul_keyboard_table_3sun, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_default, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO,
    true
};

static const HangulKeyboard hangul_keyboard_3yet = {
    (char*)"3y",
    (char*)N_("Sebeolsik Yetgeul"),
    { (ucschar*)hangul_keyboard_table_3yet, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_full, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO_YET,
    true
};

static const HangulKeyboard hangul_keyboard_romaja = {
    (char*)"ro",
    (char*)N_("Romaja"),
    { (ucschar*)hangul_keyboard_table_romaja, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_romaja, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_ROMAJA,
    true
};

static const HangulKeyboard hangul_keyboard_ahn = {
    (char*)"ahn",
    (char*)N_("Ahnmatae"),
    { (ucschar*)hangul_keyboard_table_ahn, NULL, NULL, NULL },
    { (HangulCombination*)&hangul_combination_ahn, NULL, NULL, NULL },
    HANGUL_KEYBOARD_TYPE_JASO,
    true
};

static const HangulKeyboard* hangul_builtin_keyboards[] = {
    &hangul_keyboard_2,
    &hangul_keyboard_2y,
    &hangul_keyboard_390,
    &hangul_keyboard_3final,
    &hangul_keyboard_3sun,
    &hangul_keyboard_3yet,
    &hangul_keyboard_32,
    &hangul_keyboard_romaja,
    &hangul_keyboard_ahn,
};
static unsigned int hangul_builtin_keyboard_count = countof(hangul_builtin_keyboards);

static HangulKeyboardList hangul_keyboards = { 0, 0, NULL };

typedef struct _HangulKeyboardLoadContext {
    const char* path;
    HangulKeyboard* keyboard;
    int current_id;
    const char* current_element;
    bool save_name;
} HangulKeyboardLoadContext;

static void    hangul_keyboard_parse_file(const char* path, void* user_data);
static bool    hangul_keyboard_list_append(HangulKeyboard* keyboard);


HangulCombination*
hangul_combination_new()
{
    HangulCombination *combination = malloc(sizeof(HangulCombination));
    if (combination != NULL) {
	combination->size = 0;
	combination->size_alloced = 0;
	combination->table = NULL;
	combination->is_static = false;
	return combination;
    }

    return NULL;
}

void
hangul_combination_delete(HangulCombination *combination)
{
    if (combination == NULL)
	return;

    if (combination->is_static)
	return;

    if (combination->table != NULL)
	free(combination->table);

    free(combination);
}

static uint32_t
hangul_combination_make_key(ucschar first, ucschar second)
{
    return first << 16 | second;
}

bool
hangul_combination_set_data(HangulCombination* combination,
			    ucschar* first, ucschar* second, ucschar* result,
			    unsigned int n)
{
    if (combination == NULL)
	return false;

    if (n == 0 || n > ULONG_MAX / sizeof(HangulCombinationItem))
	return false;

    combination->table = malloc(sizeof(HangulCombinationItem) * n);
    if (combination->table != NULL) {
	int i;

	combination->size = n;
	for (i = 0; i < n; i++) {
	    combination->table[i].key = hangul_combination_make_key(first[i], second[i]);
	    combination->table[i].code = result[i];
	}
	return true;
    }

    return false;
}

static bool
hangul_combination_add_item(HangulCombination* combination,
	ucschar first, ucschar second, ucschar result)
{
    if (combination == NULL)
	return false;

    if (combination->is_static)
	return false;

    if (combination->size >= combination->size_alloced) {
	size_t size_need = combination->size_alloced * 2;
	if (size_need == 0) {
	    // 처음 할당할 때에는 64개를 기본값으로 한다.
	    size_need = 64;
	}

	HangulCombinationItem* table = combination->table;
	table = realloc(table, size_need * sizeof(table[0]));
	if (table == NULL)
	    return false;

	combination->size_alloced = size_need;
	combination->table = table;
    }

    uint32_t key = hangul_combination_make_key(first, second);
    size_t i = combination->size;
    combination->table[i].key = key;
    combination->table[i].code = result;
    combination->size = i + 1;
    return true;
}

static int
hangul_combination_cmp(const void* p1, const void* p2)
{
    const HangulCombinationItem *item1 = p1;
    const HangulCombinationItem *item2 = p2;

    /* key는 unsigned int이므로 단순히 빼서 리턴하면 안된다.
     * 두 수의 차가 큰 경우 int로 변환하면서 음수가 될 수 있다. */
    if (item1->key < item2->key)
	return -1;
    else if (item1->key > item2->key)
	return 1;
    else
	return 0;
}

static void
hangul_combination_sort(HangulCombination* combination)
{
    if (combination == NULL)
	return;

    if (combination->is_static)
	return;

    qsort(combination->table, combination->size,
	sizeof(combination->table[0]), hangul_combination_cmp);
}

static ucschar
hangul_combination_combine(const HangulCombination* combination,
			   ucschar first, ucschar second)
{
    HangulCombinationItem *res;
    HangulCombinationItem key;

    if (combination == NULL)
	return 0;

    key.key = hangul_combination_make_key(first, second);
    res = bsearch(&key, combination->table, combination->size,
	          sizeof(combination->table[0]), hangul_combination_cmp);
    if (res != NULL)
	return res->code;

    return 0;
}

HangulKeyboard*
hangul_keyboard_new()
{
    HangulKeyboard *keyboard = malloc(sizeof(HangulKeyboard));
    if (keyboard == NULL)
	return NULL;

    keyboard->id = NULL;
    keyboard->name = NULL;

    keyboard->table[0] = NULL;
    keyboard->table[1] = NULL;
    keyboard->table[2] = NULL;
    keyboard->table[3] = NULL;

    keyboard->combination[0] = NULL;
    keyboard->combination[1] = NULL;
    keyboard->combination[2] = NULL;
    keyboard->combination[3] = NULL;

    keyboard->type = HANGUL_KEYBOARD_TYPE_JAMO;
    keyboard->is_static = false;

    return keyboard;
}

static void
hangul_keyboard_set_id(HangulKeyboard* keyboard, const char* id)
{
    if (keyboard == NULL)
	return;

    if (keyboard->is_static)
	return;

    free(keyboard->id);
    keyboard->id = strdup(id);
}

static void
hangul_keyboard_set_name(HangulKeyboard* keyboard, const char* name)
{
    if (keyboard == NULL)
	return;

    if (keyboard->is_static)
	return;

    free(keyboard->name);
    keyboard->name = strdup(name);
}

ucschar
hangul_keyboard_get_mapping(const HangulKeyboard* keyboard, int tableid, unsigned key)
{
    if (keyboard == NULL)
	return 0;

    if (tableid >= countof(keyboard->table))
	return 0;

    if (key >= HANGUL_KEYBOARD_TABLE_SIZE)
	return 0;

    ucschar* table = keyboard->table[tableid];
    if (table == NULL)
	return 0;

    return table[key];
}

static void
hangul_keyboard_set_mapping(HangulKeyboard *keyboard, int tableid, unsigned key, ucschar value)
{
    if (keyboard == NULL)
	return;

    if (tableid >= countof(keyboard->table))
	return;

    if (key >= HANGUL_KEYBOARD_TABLE_SIZE)
	return;

    if (keyboard->table[tableid] == NULL) {
	ucschar* new_table = malloc(sizeof(ucschar) * HANGUL_KEYBOARD_TABLE_SIZE);
	if (new_table == NULL)
	    return;

	unsigned i;
	for (i = 0; i < HANGUL_KEYBOARD_TABLE_SIZE; ++i) {
	    new_table[i] = 0;
	}
	keyboard->table[tableid] = new_table;
    }

    ucschar* table = keyboard->table[tableid];
    table[key] = value;
}

void
hangul_keyboard_set_value(HangulKeyboard *keyboard, int key, ucschar value)
{
    hangul_keyboard_set_mapping(keyboard, 0, key, value);
}

int
hangul_keyboard_get_type(const HangulKeyboard *keyboard)
{
    int type = 0;
    if (keyboard != NULL) {
	type = keyboard->type;
    }
    return type;
}

void
hangul_keyboard_set_type(HangulKeyboard *keyboard, int type)
{
    if (keyboard != NULL) {
	keyboard->type = type;
    }
}

void
hangul_keyboard_delete(HangulKeyboard *keyboard)
{
    if (keyboard == NULL)
	return;

    if (keyboard->is_static)
	return;

    free(keyboard->id);
    free(keyboard->name);

    unsigned i;
    for (i = 0; i < countof(keyboard->table); ++i) {
	if (keyboard->table[i] != NULL) {
	    free(keyboard->table[i]);
	}
    }

    for (i = 0; i < countof(keyboard->combination); ++i) {
	if (keyboard->combination[i] != NULL) {
	    hangul_combination_delete(keyboard->combination[i]);
	}
    }

    free(keyboard);
}

ucschar
hangul_keyboard_combine(const HangulKeyboard* keyboard,
	unsigned id, ucschar first, ucschar second)
{
    if (keyboard == NULL)
	return 0;

    if (id >= countof(keyboard->combination))
	return 0;

    HangulCombination* combination = keyboard->combination[id];
    ucschar res = hangul_combination_combine(combination, first, second);
    return res;
}

static const char*
attr_lookup(const char** attr, const char* name)
{
    if (attr == NULL)
	return NULL;

    int i;
    for (i = 0; attr[i] != NULL; i += 2) {
	if (strcmp(attr[i], name) == 0) {
	    return attr[i + 1];
	}
    }

    return NULL;
}

static unsigned int
attr_lookup_as_uint(const char** attr, const char* name)
{
    const char* valuestr = attr_lookup(attr, name);
    if (valuestr == NULL)
	return 0;

    unsigned int value = strtoul(valuestr, NULL, 0);
    return value;
}

static void XMLCALL
on_element_start(void* data, const XML_Char* element, const XML_Char** attr)
{
    HangulKeyboardLoadContext* context = (HangulKeyboardLoadContext*)data;

    if (strcmp(element, "hangul-keyboard") == 0) {
	if (context->keyboard != NULL) {
	    hangul_keyboard_delete(context->keyboard);
	}
	context->keyboard = hangul_keyboard_new();

	const char* id = attr_lookup(attr, "id");
	hangul_keyboard_set_id(context->keyboard, id);

	const char* typestr = attr_lookup(attr, "type");
	int type = HANGUL_KEYBOARD_TYPE_JAMO;
	if (strcmp(typestr, "jamo") == 0) {
	    type = HANGUL_KEYBOARD_TYPE_JAMO;
	} else if (strcmp(typestr, "jamo-yet") == 0) {
	    type = HANGUL_KEYBOARD_TYPE_JAMO_YET;
	} else if (strcmp(typestr, "jaso") == 0) {
	    type = HANGUL_KEYBOARD_TYPE_JASO;
	} else if (strcmp(typestr, "jaso-yet") == 0) {
	    type = HANGUL_KEYBOARD_TYPE_JASO_YET;
	} else if (strcmp(typestr, "romaja") == 0) {
	    type = HANGUL_KEYBOARD_TYPE_ROMAJA;
	}

	hangul_keyboard_set_type(context->keyboard, type);
    } else if (strcmp(element, "name") == 0) {
	if (context->keyboard == NULL)
	    return;

	const char* lang = attr_lookup(attr, "xml:lang");
	if (lang == NULL) {
	    context->save_name = true;
	} else {
	    const char* locale = setlocale(LC_ALL, NULL);
	    size_t n = strlen(lang);
	    if (strncmp(lang, locale, n) == 0) {
		context->save_name = true;
	    }
	}
	context->current_element = "name";
    } else if (strcmp(element, "map") == 0) {
	if (context->keyboard == NULL)
	    return;

	unsigned int id = attr_lookup_as_uint(attr, "id");
	if (id < countof(context->keyboard->table)) {
	    context->current_id = id;
	    context->current_element = "map";
	}
    } else if (strcmp(element, "combination") == 0) {
	if (context->keyboard == NULL)
	    return;

	unsigned int id = attr_lookup_as_uint(attr, "id");
	if (id < countof(context->keyboard->combination)) {
	    if (context->keyboard->combination[id] != NULL) {
		hangul_combination_delete(context->keyboard->combination[id]);
	    }

	    context->current_id = id;
	    context->current_element = "combination";
	    context->keyboard->combination[id] = hangul_combination_new();
	}
    } else if (strcmp(element, "item") == 0) {
	if (context->keyboard == NULL)
	    return;

	unsigned int id = context->current_id;
	if (strcmp(context->current_element, "map") == 0) {
	    HangulKeyboard* keyboard = context->keyboard;
	    unsigned int key = attr_lookup_as_uint(attr, "key");
	    unsigned int value = attr_lookup_as_uint(attr, "value");
	    hangul_keyboard_set_mapping(keyboard, id, key, value);
	} else if (strcmp(context->current_element, "combination") == 0) {
	    HangulCombination* combination = context->keyboard->combination[id];
	    unsigned int first = attr_lookup_as_uint(attr, "first");
	    unsigned int second = attr_lookup_as_uint(attr, "second");
	    unsigned int result = attr_lookup_as_uint(attr, "result");
	    hangul_combination_add_item(combination, first, second, result);
	}
    } else if (strcmp(element, "include") == 0) {
	const char* file = attr_lookup(attr, "file");
	if (file == NULL)
	    return;

	size_t n = strlen(file) + strlen(context->path) + 1;
	char* path = malloc(n);
	if (path == NULL)
	    return;

	if (file[0] == '/') {
	    strncpy(path, file, n);
	} else {
	    char* orig_path = strdup(context->path);
	    char* dir = dirname(orig_path);
	    snprintf(path, n, "%s/%s", dir, file);
	    free(orig_path);
	}

	hangul_keyboard_parse_file(path, context);
	free(path);
    }
}

static void XMLCALL
on_element_end(void* data, const XML_Char* element)
{
    HangulKeyboardLoadContext* context = (HangulKeyboardLoadContext*)data;

    if (context->keyboard == NULL)
	return;

    if (strcmp(element, "name") == 0) {
	context->current_element = "";
	context->save_name = false;
    } else if (strcmp(element, "map") == 0) {
	context->current_id = 0;
	context->current_element = "";
    } else if (strcmp(element, "combination") == 0) {
	unsigned int id = context->current_id;
	HangulCombination* combination = context->keyboard->combination[id];
	hangul_combination_sort(combination);
	context->current_id = 0;
	context->current_element = "";
    }
}

static void XMLCALL
on_char_data(void* data, const XML_Char* s, int len)
{
    HangulKeyboardLoadContext* context = (HangulKeyboardLoadContext*)data;

    if (context->keyboard == NULL)
	return;

    if (strcmp(context->current_element, "name") == 0) {
	if (context->save_name) {
	    char buf[1024];
	    if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	    memcpy(buf, s, len);
	    buf[len] = '\0';
	    hangul_keyboard_set_name(context->keyboard, buf);
	}
    }
}

static void
hangul_keyboard_parse_file(const char* path, void* user_data)
{
    XML_Parser parser = XML_ParserCreate(NULL);

    XML_SetUserData(parser, user_data);
    XML_SetElementHandler(parser, on_element_start, on_element_end);
    XML_SetCharacterDataHandler(parser, on_char_data);

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        goto done;
    }

    char buf[8192];

    while (true) {
	size_t n = fread(buf, 1, sizeof(buf), file);
	int is_final = feof(file);
	int res = XML_Parse(parser, buf, n, is_final);
	if (res == XML_STATUS_ERROR) {
	    goto close;
	}
	if (is_final)
	    break;
    }

close:
    fclose(file);
done:
    XML_ParserFree(parser);
}

static HangulKeyboard*
hangul_keyboard_new_from_file(const char* path)
{
    HangulKeyboardLoadContext context = { path, NULL, 0, "" };

    hangul_keyboard_parse_file(path, &context);

    return context.keyboard;
}

static unsigned
hangul_keyboard_list_load_dir(const char* path)
{
    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s/*.xml", path);

    glob_t result;
    int res = glob(pattern, GLOB_ERR, NULL, &result);
    if (res != 0)
	return 0;

    size_t i;
    for (i = 0; i < result.gl_pathc; ++i) {
	HangulKeyboard* keyboard = hangul_keyboard_new_from_file(result.gl_pathv[i]);
	if (keyboard == NULL)
	    continue;
	hangul_keyboard_list_append(keyboard);
    }

    globfree(&result);

    return hangul_keyboards.n;
}

static void
hangul_keyboard_list_clear()
{
    size_t i;
    for (i = 0; i < hangul_keyboards.n; ++i) {
	hangul_keyboard_delete(hangul_keyboards.keyboards[i]);
    }

    free(hangul_keyboards.keyboards);

    hangul_keyboards.n = 0;
    hangul_keyboards.nalloced = 0;
    hangul_keyboards.keyboards = NULL;
}

int
hangul_keyboard_list_init()
{
    /* hangul_init을 호출하면 builtin keyboard는 disable되도록 처리한다.
     * 기본 자판은 외부 파일로 부터 로딩하는 것이 기본 동작이고
     * builtin 키보드는 하위 호환을 위해 남겨둔다. */
    hangul_builtin_keyboard_count = 0;

    unsigned n = 0;
    /* libhangul data dir에서 keyboard 로딩 */
    n += hangul_keyboard_list_load_dir(LIBHANGUL_KEYBOARD_DIR);

    /* 유저의 개별 키보드 파일 로딩 */
    char user_data_dir[PATH_MAX];
    char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home == NULL) {
	char* home_dir = getenv("HOME");
	snprintf(user_data_dir, sizeof(user_data_dir),
		"%s/.local/share/libhangul/keyboards", home_dir);
    } else {
	snprintf(user_data_dir, sizeof(user_data_dir),
		"%s/libhangul/keyboards", xdg_data_home);
    }
    n += hangul_keyboard_list_load_dir(user_data_dir);

    if (n == 0)
	return 1;

    return 0;
}

int
hangul_keyboard_list_fini()
{
    hangul_keyboard_list_clear();
    hangul_builtin_keyboard_count = countof(hangul_builtin_keyboards);
    return 0;
}

static char*
hangul_builtin_keyboard_list_get_keyboard_id(unsigned index_)
{
    if (index_ >= hangul_builtin_keyboard_count)
	return NULL;

    const HangulKeyboard* keyboard = hangul_builtin_keyboards[index_];
    if (keyboard == NULL)
	return NULL;

    return keyboard->id;
}

static const char*
hangul_builtin_keyboard_list_get_keyboard_name(unsigned index_)
{
#ifdef ENABLE_NLS
    static bool isGettextInitialized = false;
    if (!isGettextInitialized) {
	isGettextInitialized = true;
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    }
#endif

    if (index_ >= hangul_builtin_keyboard_count)
	return NULL;

    const HangulKeyboard* keyboard = hangul_builtin_keyboards[index_];
    if (keyboard == NULL)
	return NULL;

    return keyboard->name;
}

static const HangulKeyboard*
hangul_builtin_keyboard_list_get_keyboard(const char* id)
{
    size_t i;
    for (i = 0; i < hangul_builtin_keyboard_count; ++i) {
	const HangulKeyboard* keyboard = hangul_builtin_keyboards[i];
	if (strcmp(id, keyboard->id) == 0) {
	    return keyboard;
	}
    }
    return NULL;
}

unsigned int
hangul_keyboard_list_get_count()
{
    if (hangul_builtin_keyboard_count > 0)
	return hangul_builtin_keyboard_count;

    return hangul_keyboards.n;
}

const char*
hangul_keyboard_list_get_keyboard_id(unsigned index_)
{
    if (hangul_builtin_keyboard_count > 0) {
	return hangul_builtin_keyboard_list_get_keyboard_id(index_);
    }

    if (index_ >= hangul_keyboards.n)
	return NULL;

    HangulKeyboard* keyboard = hangul_keyboards.keyboards[index_];
    if (keyboard == NULL)
	return NULL;

    return keyboard->id;
}

const char*
hangul_keyboard_list_get_keyboard_name(unsigned index_)
{
    if (hangul_builtin_keyboard_count > 0) {
	return hangul_builtin_keyboard_list_get_keyboard_name(index_);
    }

    if (index_ >= hangul_keyboards.n)
	return NULL;

    HangulKeyboard* keyboard = hangul_keyboards.keyboards[index_];
    if (keyboard == NULL)
	return NULL;

    return keyboard->name;
}

const HangulKeyboard*
hangul_keyboard_list_get_keyboard(const char* id)
{
    if (hangul_builtin_keyboard_count > 0) {
	return hangul_builtin_keyboard_list_get_keyboard(id);
    }

    size_t i;
    for (i = 0; i < hangul_keyboards.n; ++i) {
	HangulKeyboard* keyboard = hangul_keyboards.keyboards[i];
	if (strcmp(id, keyboard->id) == 0) {
	    return keyboard;
	}
    }
    return NULL;
}

static bool
hangul_keyboard_list_append(HangulKeyboard* keyboard)
{
    if (hangul_keyboards.n >= hangul_keyboards.nalloced) {
	size_t n = hangul_keyboards.nalloced * 2;
	if (n == 0) {
	    n = 16;
	}
	HangulKeyboard** keyboards = hangul_keyboards.keyboards;
	keyboards = realloc(keyboards, n * sizeof(keyboards[0]));
	if (keyboards == NULL)
	    return false;

	hangul_keyboards.nalloced = n;
	hangul_keyboards.keyboards = keyboards;
    }

    size_t i = hangul_keyboards.n;
    hangul_keyboards.keyboards[i] = keyboard;
    hangul_keyboards.n = i + 1;

    return true;
}