#include <notcurses/nckeys.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <notcurses/notcurses.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define NTOOLS (sizeof(TOOLS) / sizeof(TOOLS[0]))

enum tooltype {
	TOOL_DRAW,
	TOOL_PIPETTE,
};

enum dialogtype {
	DIALOG_NONE,
	DIALOG_TOOL,
};

struct g {
	struct notcurses *nc;
	struct ncplane *stdp;
	int termw, termh;
	struct nctabbed *tabbed;
};

struct editor {
	char *filepath;
	int w, h;
	uint8_t (*data)[4]; // always RGBA
	enum tooltype tool;
	enum dialogtype dialog;
	int viewx, viewy;
	int curx, cury; // not relative to view{x,y}
	uint8_t primr, primg, primb, prima;
	uint8_t secr, secg, secb, seca;
};

struct tool {
	const char *name;
	void (*fn)(struct editor *, struct ncinput *);
};

static void init();
static void main_loop();
static void open_dialog_tool(struct editor *ed);
static int open_file(const char *filepath);
static void tab_callback(struct nctab* tab, struct ncplane* ncp, void* curry);
static void toolfn_draw(struct editor *ed, struct ncinput *ni);
static void toolfn_pipette(struct editor *ed, struct ncinput *ni);

struct tool TOOLS[] = {
	[TOOL_DRAW] = { "draw", toolfn_draw },
	[TOOL_PIPETTE] = { "pipette", toolfn_pipette },
};

struct g g;

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
}

void
main_loop()
{
	struct ncinput ni;
	while (1) {
		nctabbed_redraw(g.tabbed);
		notcurses_render(g.nc);
		notcurses_getc_blocking(g.nc, &ni);
		struct editor *ed = nctab_userptr(nctabbed_selected(g.tabbed));
		if (nckey_mouse_p(ni.id)) {
			int xlim = g.termw;
			if (xlim > ed->w - ed->viewx) xlim = ed->w - ed->viewx;
			int ylim = g.termh;
			if (ylim > 2 + ed->h - ed->viewy) ylim = 2 + ed->h - ed->viewy;
			if (ni.y > 1 && ni.x < xlim && ni.y < ylim) {
				ed->curx = ed->viewx + ni.x;
				ed->cury = ed->viewy + ni.y - 2;
				TOOLS[ed->tool].fn(ed, &ni);
			}
		} else {
			switch (ni.id) {
				case 'A': {
					if (ed->viewx > 0) --ed->viewx;
					if (ed->curx > ed->viewx + g.termw - 1) --ed->curx;
				} break;
				case 'D': {
					if (ed->viewx < ed->w - 1) ++ed->viewx;
					if (ed->curx < ed->viewx) ++ed->curx;
				} break;
				case 'W': {
					if (ed->viewy > 0) --ed->viewy;
					if (ed->cury > ed->viewy + g.termh - 2 - 1) --ed->cury;
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
					if (ed->curx > ed->viewx + g.termw - 1) ++ed->viewx;
				} break;
				case 'w': {
					if (ed->cury > 0) --ed->cury;
					if (ed->cury < ed->viewy) --ed->viewy;
				} break;
				case 's': {
					if (ed->cury < ed->h - 1) ++ed->cury;
					if (ed->cury > ed->viewy + g.termh - 2 - 1) ++ed->viewy;
				} break;
				case 't': {
					open_dialog_tool(ed);
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
open_dialog_tool(struct editor *ed)
{
	(void) ed;
	// TODO: either use ncselector or write a thing
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
	ed->prima = ed->seca = 255;
	
	struct nctab *tab = nctabbed_add(g.tabbed, NULL, NULL, tab_callback, filepath, ed);
	(void) tab;
	
	return 0;
}

void
tab_callback(struct nctab* tab, struct ncplane* ncp, void* curry)
{
	(void) tab;
	struct editor *ed = curry;
	ncplane_erase(ncp);
	
	ncplane_printf(ncp, " α %-3d", ed->data[ed->curx + ed->cury * ed->w][3]);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_set_bg_rgb8(ncp, ed->primr, ed->primg, ed->primb);
	ncplane_putstr(ncp, "  ");
	ncplane_set_bg_default(ncp);
	ncplane_printf(ncp, " α %-3d", ed->prima);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_set_bg_rgb8(ncp, ed->secr, ed->secg, ed->secb);
	ncplane_putstr(ncp, "  ");
	ncplane_set_bg_default(ncp);
	ncplane_printf(ncp, " α %-3d", ed->seca);
	
	ncplane_putstr(ncp, "   ");
	
	ncplane_putstr(ncp, TOOLS[ed->tool].name);
	
	int yupto = g.termh - 2;
	if (yupto > ed->h - ed->viewy) yupto = ed->h - ed->viewy;
	int xupto = g.termw;
	if (xupto > ed->w - ed->viewx) xupto = ed->w - ed->viewx;
	for (int y = 0; y < yupto; ++y) {
		for (int x = 0; x < xupto; ++x) {
			int ix = x + ed->viewx;
			int iy = y + ed->viewy;
			ncplane_set_bg_rgb8(ncp,
				ed->data[ix + iy*ed->w][0],
				ed->data[ix + iy*ed->w][1],
				ed->data[ix + iy*ed->w][2]
			);
			if (iy == ed->cury && ix == ed->curx) {
				ncplane_set_fg_rgb8(ncp,
					255 - ed->data[ix + iy*ed->w][0],
					255 - ed->data[ix + iy*ed->w][1],
					255 - ed->data[ix + iy*ed->w][2]
				);
				ncplane_putchar_yx(ncp, y + 1, x, 'X');
				ncplane_set_fg_default(ncp);
			} else {
				ncplane_putchar_yx(ncp, y + 1, x, ' ');
			}
		}
	}
	ncplane_set_bg_default(ncp);
}

void
toolfn_draw(struct editor *ed, struct ncinput *ni)
{
	if (nckey_mouse_p(ni->id)) {
		if (ni->id == NCKEY_BUTTON1) {
			goto prim;
		} else if (ni->id != NCKEY_RELEASE) {
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
	ed->data[ed->curx + ed->cury * ed->w][0] = ed->primr;
	ed->data[ed->curx + ed->cury * ed->w][1] = ed->primg;
	ed->data[ed->curx + ed->cury * ed->w][2] = ed->primb;
	ed->data[ed->curx + ed->cury * ed->w][3] = ed->prima;
	return;
sec:
	ed->data[ed->curx + ed->cury * ed->w][0] = ed->secr;
	ed->data[ed->curx + ed->cury * ed->w][1] = ed->secg;
	ed->data[ed->curx + ed->cury * ed->w][2] = ed->secb;
	ed->data[ed->curx + ed->cury * ed->w][3] = ed->seca;
}

void
toolfn_pipette(struct editor *ed, struct ncinput *ni)
{
	if (nckey_mouse_p(ni->id)) {
		if (ni->id == NCKEY_BUTTON1) {
			goto prim;
		} else if (ni->id != NCKEY_RELEASE) {
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
	ed->primr = ed->data[ed->curx + ed->cury * ed->w][0];
	ed->primg = ed->data[ed->curx + ed->cury * ed->w][1];
	ed->primb = ed->data[ed->curx + ed->cury * ed->w][2];
	ed->prima = ed->data[ed->curx + ed->cury * ed->w][3];
	return;
sec:
	ed->secr = ed->data[ed->curx + ed->cury * ed->w][0];
	ed->secg = ed->data[ed->curx + ed->cury * ed->w][1];
	ed->secb = ed->data[ed->curx + ed->cury * ed->w][2];
	ed->seca = ed->data[ed->curx + ed->cury * ed->w][3];
}

int
main()
{
	init();
//	open_file("/home/masflam/pics/masflam-xmr-qr.png");
	open_file("/home/masflam/pics/Head_THINK.png");
	main_loop();
	return 0;
}
