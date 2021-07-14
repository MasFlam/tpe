#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define DIALOG_BG CHANNEL_RGB_INITIALIZER(32, 32, 32)
#define DIALOG_BG_SEL CHANNEL_RGB_INITIALIZER(64, 64, 64)
#define MAX_FORMAT_NAME_LEN 3
#define MAX_TOOL_NAME_LEN 7
#define NFORMATS (sizeof(FORMATS) / sizeof(FORMATS[0]))
#define NTOOLS (sizeof(TOOLS) / sizeof(TOOLS[0]))

enum tooltype {
	TOOL_DRAW,
	TOOL_PIPETTE,
};

enum format {
	FORMAT_PNG,
	FORMAT_BMP,
	FORMAT_TGA,
	FORMAT_JPG,
};

// No nasty alignment problems, please
#pragma pack (push, 1)
struct rgba {
	uint8_t r, g, b, a;
};
#pragma pack (pop)

struct g {
	struct notcurses *nc;
	struct ncplane *stdp;
	int termw, termh;
	struct nctabbed *tabbed;
	struct ncplane *viewplane;
	int vieww, viewh; // this is the actual width and height of viewplane in cells
	char *message;
};

struct editor {
	char *filepath;
	int w, h;
	struct rgba *data;
	enum tooltype tool;
	int viewx, viewy;
	int curx, cury; // not relative to view{x,y}
	struct rgba pricol, seccol;
};

struct tool {
	const char *name;
	void (*fn)(struct editor *, struct ncinput *);
};

struct formatdata {
	const char *name;
	int (*savefn)(char const *, int, int, int, const void *);
};

static void cleanup();
static int dialog_save(struct editor *ed);
static int dialog_tool(struct editor *ed);
static void init();
static void main_loop();
static void message(const char *msg);
static int open_file(const char *filepath);
static void tab_callback(struct nctab* tab, struct ncplane* ncp, void* curry);
static void toolfn_draw(struct editor *ed, struct ncinput *ni);
static void toolfn_pipette(struct editor *ed, struct ncinput *ni);
static int savefn_jpg(char const *filepath, int w, int h, int comp, const void *data);
static int savefn_png(char const *filepath, int w, int h, int comp, const void *data);

struct tool TOOLS[] = {
	[TOOL_DRAW] = { "draw", toolfn_draw },
	[TOOL_PIPETTE] = { "pipette", toolfn_pipette },
};

struct formatdata FORMATS[] = {
	[FORMAT_PNG] = { "PNG", savefn_png },
	[FORMAT_BMP] = { "BMP", stbi_write_bmp },
	[FORMAT_TGA] = { "TGA", stbi_write_tga },
	[FORMAT_JPG] = { "JPG", savefn_jpg },
};

struct g g;

void
cleanup()
{
	struct nctab *tab;
	while ((tab = nctabbed_selected(g.tabbed))) {
		struct editor *ed = nctab_userptr(tab);
		stbi_image_free(ed->data);
		free(ed->filepath);
		free(ed);
		nctabbed_del(g.tabbed, tab);
	}
	nctabbed_destroy(g.tabbed);
	free(g.message);
	notcurses_stop(g.nc);
}

int
dialog_save(struct editor *ed)
{
	int ret = 0;
	struct ncplane *ncp = ncplane_create(g.stdp, &(struct ncplane_options) {
		.x = 1,
		.y = 2,
		.cols = MAX_FORMAT_NAME_LEN,
		.rows = NFORMATS
	});
	struct nccell basecell = {};
	ncplane_base(ncp, &basecell);
	ncchannels_set_bchannel(&basecell.channels, DIALOG_BG);
	ncplane_set_base_cell(ncp, &basecell);
	
	int sel = FORMAT_PNG;
	struct ncinput ni;
	while (1) {
		ncplane_erase(ncp);
		for (int i = 0; i < NFORMATS; ++i) {
			if (i == sel) ncplane_set_bchannel(ncp, DIALOG_BG_SEL);
			ncplane_printf_yx(ncp, i, 0, "%-*s", MAX_FORMAT_NAME_LEN, FORMATS[i].name);
			if (i == sel) ncplane_set_bg_default(ncp);
		}
		notcurses_render(g.nc);
		notcurses_getc_blocking(g.nc, &ni);
		
		switch (ni.id) {
		case NCKEY_ESC:
		case NCKEY_BACKSPACE: {
			goto end;
		} break;
		case NCKEY_SPACE:
		case NCKEY_ENTER: {
			goto saveit;
		} break;
		case NCKEY_BUTTON1: {
			if (ni.x >= 1 && ni.x < 1 + MAX_FORMAT_NAME_LEN) {
				int i = ni.y - 2;
				if (i >= 0 && i < NFORMATS) {
					sel = i;
				}
			}
			goto saveit;
		} break;
		case 'w':
		case NCKEY_UP:
		case NCKEY_SCROLL_UP: {
			sel = (sel + NFORMATS - 1) % NFORMATS;
		} break;
		case 's':
		case NCKEY_DOWN:
		case NCKEY_SCROLL_DOWN: {
			sel = (sel + 1) % NFORMATS;
		} break;
		}
	}
saveit:
	if (!FORMATS[sel].savefn(ed->filepath, ed->w, ed->h, 4, ed->data)) {
		ret = -1;
	} else {
		ret = 1;
	}
end:;
	ncplane_destroy(ncp);
	return ret;
}

int
dialog_tool(struct editor *ed)
{
	int ret = 0;
	struct ncplane *ncp = ncplane_create(g.stdp, &(struct ncplane_options) {
		.x = 1,
		.y = 2,
		.cols = MAX_TOOL_NAME_LEN,
		.rows = NTOOLS
	});
	struct nccell basecell = {};
	ncplane_base(ncp, &basecell);
	ncchannels_set_bchannel(&basecell.channels, DIALOG_BG);
	ncplane_set_base_cell(ncp, &basecell);
	
	int sel = ed->tool;
	struct ncinput ni;
	while (1) {
		ncplane_erase(ncp);
		for (int i = 0; i < NTOOLS; ++i) {
			if (i == sel) ncplane_set_bchannel(ncp, DIALOG_BG_SEL);
			ncplane_printf_yx(ncp, i, 0, "%-*s", MAX_TOOL_NAME_LEN, TOOLS[i].name);
			if (i == sel) ncplane_set_bg_default(ncp);
		}
		notcurses_render(g.nc);
		notcurses_getc_blocking(g.nc, &ni);
		
		switch (ni.id) {
		case NCKEY_ESC:
		case NCKEY_BACKSPACE: {
			goto end;
		} break;
		case NCKEY_SPACE:
		case NCKEY_ENTER: {
			ed->tool = sel;
			ret = 1;
			goto end;
		} break;
		case NCKEY_BUTTON1: {
			if (ni.x >= 1 && ni.x < 1 + MAX_TOOL_NAME_LEN) {
				int i = ni.y - 2;
				if (i >= 0 && i < NTOOLS) {
					sel = i;
				}
			}
			ed->tool = sel;
			ret = 1;
			goto end;
		} break;
		case 'w':
		case NCKEY_UP:
		case NCKEY_SCROLL_UP: {
			sel = (sel + NTOOLS - 1) % NTOOLS;
		} break;
		case 's':
		case NCKEY_DOWN:
		case NCKEY_SCROLL_DOWN: {
			sel = (sel + 1) % NTOOLS;
		} break;
		}
	}
end:
	ncplane_destroy(ncp);
	return ret;
}

void
init()
{
	g.nc = notcurses_core_init(&(struct notcurses_options) {
		.flags = NCOPTION_SUPPRESS_BANNERS
	}, stdout);
	g.stdp = notcurses_stddim_yx(g.nc, &g.termh, &g.termw);
	notcurses_mouse_enable(g.nc);
	
	struct ncplane *ncp = ncplane_create(g.stdp, &(struct ncplane_options) {
		.x = 0, .y = 0,
		.cols = g.termw,
		.rows = g.termh
	});
	g.tabbed = nctabbed_create(ncp, &(struct nctabbed_options) {
		.selchan = CHANNELS_RGB_INITIALIZER(0, 0, 0, 220, 220, 220),
		.separator = " "
	});
	
	struct ncplane *tabp = nctabbed_content_plane(g.tabbed);
	g.viewplane = ncplane_create(tabp, &(struct ncplane_options) {
		.x = 0, .y = 1,
		.cols = g.vieww = g.termw,
		.rows = g.viewh = g.termh - 2
	});
}

void
main_loop()
{
	struct ncinput ni;
	struct editor *ed = nctab_userptr(nctabbed_selected(g.tabbed));
	while (1) {
		nctabbed_ensure_selected_header_visible(g.tabbed);
		nctabbed_redraw(g.tabbed);
		notcurses_render(g.nc);
		notcurses_getc_blocking(g.nc, &ni);
		if (nckey_mouse_p(ni.id)) {
			int xlim = g.vieww * 2;
			if (xlim > (ed->w - ed->viewx) * 2) xlim = (ed->w - ed->viewx) * 2;
			int ylim = 2 + g.viewh;
			if (ylim > 2 + ed->h - ed->viewy) ylim = 2 + ed->h - ed->viewy;
			if (ni.y > 1 && ni.x < xlim && ni.y < ylim) {
				if (ni.id != NCKEY_RELEASE) {
					ed->curx = ed->viewx + ni.x/2;
					ed->cury = ed->viewy + ni.y - 2;
				}
				TOOLS[ed->tool].fn(ed, &ni);
			}
		} else if (ni.ctrl || ni.alt) {
			switch (ni.id) {
			case 's':
			case 'S': {
				int r = dialog_save(ed);
				if (r < 0) {
					message("Saving failed");
				} else if (r > 0) {
					message("Saved");
				} else {
					message("Saving cancelled");
				}
			} break;
			}
		} else {
			switch (ni.id) {
			case 'q': {
				free(ed->filepath);
				stbi_image_free(ed->data);
				free(ed);
				nctabbed_del(g.tabbed, nctabbed_selected(g.tabbed));
				if (nctabbed_tabcount(g.tabbed) == 0) {
					return;
				} else {
					ed = nctab_userptr(nctabbed_selected(g.tabbed));
				}
			} break;
			case NCKEY_LEFT: {
				ed = nctab_userptr(nctabbed_prev(g.tabbed));
			} break;
			case NCKEY_RIGHT: {
				ed = nctab_userptr(nctabbed_next(g.tabbed));
			} break;
			case 'A': {
				if (ed->viewx > 0) --ed->viewx;
				if (ed->curx > ed->viewx + g.vieww/2 - 1) --ed->curx;
			} break;
			case 'D': {
				if (ed->viewx < ed->w - 1) ++ed->viewx;
				if (ed->curx < ed->viewx) ++ed->curx;
			} break;
			case 'W': {
				if (ed->viewy > 0) --ed->viewy;
				if (ed->cury > ed->viewy + g.viewh - 1) --ed->cury;
			} break;
			case 'S': {
				if (ed->viewy < ed->h - 1) ++ed->viewy;
				if (ed->cury < ed->viewy) ++ed->cury;
			} break;
			case 'a': {
				if (ed->curx > 0) --ed->curx;
				if (ed->curx < ed->viewx) --ed->viewx;
			} break;
			case 'd': {
				if (ed->curx < ed->w - 1) ++ed->curx;
				if (ed->curx > ed->viewx + g.vieww/2 - 1) ++ed->viewx;
			} break;
			case 'w': {
				if (ed->cury > 0) --ed->cury;
				if (ed->cury < ed->viewy) --ed->viewy;
			} break;
			case 's': {
				if (ed->cury < ed->h - 1) ++ed->cury;
				if (ed->cury > ed->viewy + g.viewh - 1) ++ed->viewy;
			} break;
			case 't': {
				int r = dialog_tool(ed);
				if (r > 0) {
					message("Selected tool");
				} else if (r < 0) {
					message("Failed to select tool");
				} else {
					message("Tool selection cancelled");
				}
			} break;
			case '1': {
				ed->tool = TOOL_DRAW;
			} break;
			case '2': {
				ed->tool = TOOL_PIPETTE;
			} break;
			case NCKEY_SPACE:
			case NCKEY_ENTER: {
				TOOLS[ed->tool].fn(ed, &ni);
			} break;
			}
		}
	}
}

void
message(const char *msg)
{
	if (g.message) free(g.message);
	g.message = strdup(msg);
}

int
open_file(const char *filepath)
{
	int w, h;
	void *data = stbi_load(filepath, &w, &h, NULL, 4);
	if (!data) return -1;
	
	struct editor *ed = calloc(1, sizeof(struct editor));
	ed->filepath = strdup(filepath);
	ed->w = w;
	ed->h = h;
	ed->data = data;
	ed->pricol.a = ed->seccol.a = 255;
	
	struct nctab *tab = nctabbed_add(g.tabbed, NULL, NULL, tab_callback, filepath, ed);
	nctabbed_select(g.tabbed, tab);
	
	return 0;
}

void
tab_callback(struct nctab* tab, struct ncplane* ncp, void* curry)
{
	(void) tab;
	struct editor *ed = curry;
	ncplane_erase(ncp);
	
	ncplane_printf(ncp, " α %-3d", ed->data[ed->curx + ed->cury * ed->w].a);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_set_bg_rgb8(ncp, ed->pricol.r, ed->pricol.g, ed->pricol.b);
	ncplane_putstr(ncp, "  ");
	ncplane_set_bg_default(ncp);
	ncplane_printf(ncp, " α %-3d", ed->pricol.a);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_set_bg_rgb8(ncp, ed->seccol.r, ed->seccol.g, ed->seccol.b);
	ncplane_putstr(ncp, "  ");
	ncplane_set_bg_default(ncp);
	ncplane_printf(ncp, " α %-3d", ed->seccol.a);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_putstr(ncp, TOOLS[ed->tool].name);
	
	if (g.message) {
		ncplane_putstr_yx(
			nctabbed_content_plane(g.tabbed),
			0, g.termw - ncstrwidth(g.message),
			g.message
		);
		free(g.message);
		g.message = NULL;
	}
	
	ncplane_erase(g.viewplane);
	int yupto = g.viewh;
	if (yupto > ed->h - ed->viewy) yupto = ed->h - ed->viewy;
	int xupto = g.vieww / 2;
	if (xupto > ed->w - ed->viewx) xupto = ed->w - ed->viewx;
	for (int y = 0; y < yupto; ++y) {
		for (int x = 0; x < xupto; ++x) {
			int ix = x + ed->viewx;
			int iy = y + ed->viewy;
			struct rgba rgba = ed->data[ix + iy*ed->w];
			ncplane_set_bg_rgb8(g.viewplane, rgba.r, rgba.g, rgba.b);
			if (iy == ed->cury && ix == ed->curx) {
				ncplane_set_fg_rgb8(g.viewplane, 255 - rgba.r, 255 - rgba.g, 255 - rgba.b);
				ncplane_putstr_yx(g.viewplane, y, 2*x, "[]");
				ncplane_set_fg_default(g.viewplane);
			} else {
				ncplane_putstr_yx(g.viewplane, y, 2*x, "  ");
			}
		}
	}
}

void
toolfn_draw(struct editor *ed, struct ncinput *ni)
{
	if (nckey_mouse_p(ni->id)) {
		if (ni->id == NCKEY_BUTTON1) {
			goto prim;
		} else if (ni->id == NCKEY_BUTTON2 || ni->id == NCKEY_BUTTON3) {
			goto sec;
		}
	} else {
		if (ni->id == NCKEY_ENTER) {
			goto prim;
		} else {
			goto sec;
		}
	}
	return;
prim:
	ed->data[ed->curx + ed->cury * ed->w] = ed->pricol;
	return;
sec:
	ed->data[ed->curx + ed->cury * ed->w] = ed->seccol;
}

void
toolfn_pipette(struct editor *ed, struct ncinput *ni)
{
	if (nckey_mouse_p(ni->id)) {
		if (ni->id == NCKEY_BUTTON1) {
			goto prim;
		} else if (ni->id == NCKEY_BUTTON2 || ni->id == NCKEY_BUTTON3) {
			goto sec;
		}
	} else {
		if (ni->id == NCKEY_ENTER) {
			goto prim;
		} else {
			goto sec;
		}
	}
	return;
prim:
	ed->pricol = ed->data[ed->curx + ed->cury * ed->w];
	return;
sec:
	ed->seccol = ed->data[ed->curx + ed->cury * ed->w];
}

int
savefn_jpg(char const *filepath, int w, int h, int comp, const void *data)
{
	return stbi_write_png(filepath, w, h, comp, data, w * comp);
}

int
savefn_png(char const *filepath, int w, int h, int comp, const void *data)
{
	return stbi_write_jpg(filepath, w, h, comp, data, 95);
}

int
main(int argc, char **argv)
{
	init();
	int optsdone = 0;
	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		if (arg[0] != '-' || optsdone) {
			open_file(arg);
		} else if (arg[0] == '-' && arg[1] == '-') {
			char *lopt = &arg[2];
			if (lopt[0] == '\0') {
				optsdone = 1;
			}
		} else if (arg[1] != '-') {
			char *sopts = &arg[1];
		}
	}
	if (nctabbed_tabcount(g.tabbed) == 0) {
		cleanup();
		return 1;
	}
	nctabbed_select(g.tabbed, nctabbed_leftmost(g.tabbed));
	main_loop();
	cleanup();
	return 0;
}
