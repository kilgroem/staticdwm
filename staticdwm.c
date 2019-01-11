/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
/* Include {{{*/
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include "drw.h"
#include "util.h"
/*}}}*/
/* Macros {{{*/
#define NFRAMES					(MAXTILEDFRAMES + 1)
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) &\
		(ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww)\
								- MAX((x),(m)->wx)) \
								* MAX(0, MIN((y)+(h),(m)->wy+(m)->wh)\
								* - MAX((y),(m)->wy)))
#define SELECTED(M)				((M)->frames[(M)->selfrm].last)
#define FOCUSED(M)				((M)->frames[(M)->focfrm].last)
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define SETBORDERCOL(X)		 	XSetWindowBorder(dpy, (X)->win,\
			scheme[c->isfloating ? SchemeFlFoc : SchemeFoc][ColBorder].pixel);
#define ColBorder               2
/*}}}*/
/* Enumerators {{{*/
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeFoc,
	   SchemeFlNorm, SchemeFlSel, SchemeFlFoc}; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };/* default atoms */
enum { ClkTagBar, ClkFrmBar, ClkStatusText, ClkWinTitle, ClkClientWin,
	   ClkRootWin, ClkLast, ClkFrm }; /* clicks */
/*}}}*/
/* Forward definitions {{{*/
typedef struct Monitor Monitor;
typedef struct ClientLink ClientLink;
typedef struct Client Client;
typedef struct Frame Frame;
/*}}}*/
/* Structures needed for config.h {{{*/
/* Arg {{{*/
typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;
/*}}}*/
/* Button {{{*/
typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;
/*}}}*/
/* Key {{{*/
typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;
/*}}}*/
/* Rule {{{*/
typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	int tag;
	int isfloating;
	int monitor;
} Rule;
/*}}}*/
/*}}}*/
/* Function Declarations {{{*/
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h,
							int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachfocus(Client * c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static int clicktoframe(Monitor *m, float fx, float fy);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void exchangeframecontents(unsigned int a, unsigned int b);
static void expose(XEvent *e);
static void refocus(void);
static void focusclient(Client *c);
static void focusin(XEvent *e);
static void focusnothing(void);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int isavailable(Client * c);
static int isfloating(Client * c);
static int isinfrm(Client * c);
static void keypress(XEvent *e);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void propertynotify(XEvent *e);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void restack(Monitor *m);
static void restacksel(void);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static Client * selwinforselfrm(Monitor * m, int dir);
static void setclientstate(Client *c, long state);
static void setfocfrm(Monitor * m, unsigned int frm);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setopenframes(Monitor *m, unsigned int nf);
static void setselfrm(Monitor * m, unsigned int frm);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateborder(Client *c, int bw);
static void updateclientlist(void);
static void updatefrmpos(Monitor * m);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatewindowtype(Client *c);
static void updatetitle(Client *c);
static void updatewmhints(Client *c);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
/*}}}*/
/* User Called Function Declarations {{{*/
static void closeframe(const Arg *arg);
static void closeframebelow(const Arg *arg);
static void emptyframe(const Arg *arg);
static void fillframe(const Arg *arg);
static void focusframe(const Arg *arg);
static void focusmon(const Arg *arg);
static void killclient(const Arg *arg);
static void movemouse(const Arg *arg);
static void onlyframe(const Arg *arg);
static void quit(const Arg *arg);
static void resizemouse(const Arg *arg);
static void selectframe(const Arg *arg);
static void spawn(const Arg *arg);
static void swapfocus(const Arg *arg);
static void swapframe(const Arg *arg);
static void tag(const Arg *arg);
static void tagandview(const Arg *arg);
static void tagmon(const Arg *arg);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggleframe(const Arg *arg);
static void view(const Arg *arg);
/*}}}*/
/* Include config.h {{{*/
#include "config.h"
/*}}}*/
/* Other Structures {{{*/
/* Frame {{{*/
struct Frame {
	unsigned int tag;
	int x;
	int y;
	int w;
	int h;
	Client * last;
	Monitor * mon;
};
/*}}}*/
/* Client {{{*/
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tag, lastfrm;
	int isfixed, isurgent, neverfocus, isfloating, isfullscreen;
	Client *next, *snext, *sprev, *focusto;
	ClientLink *focusfrom;
	Monitor *mon;
	Window win;
};/*}}}*/
/* Monitor {{{*/
struct Monitor {
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	/* max x positions for bar features to turn click position to action */
	int btagx[NTAGS], bfrmx[NFRAMES], bseltitlex;
	int nopenfrms;
	unsigned int selfrm, focfrm, selfrmold, focfrmold;
	int showbar;
	int topbar;
	Client *clients;
	Client *stack;
	Client *stacklast;
	Monitor *next;
	Window barwin;
	Frame frames[NFRAMES];
};
/*}}}*/
/* ClientLink {{{*/
struct ClientLink {
	Client * c;
	ClientLink * prev;
	ClientLink * next;
};/*}}}*/
/*}}}*/
/* Variables {{{*/
static const char broken[] = "broken";
static char stext[256];
static size_t nframehooks;
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh; /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
/*}}}*/
/* Compile time check on tags and frame sizes. {{{*/
struct NumTags {char taglimitexceeded[NTAGS > 31 ? -1 : 1]; 
				char atleasttwotagsrequired[NTAGS < 2 ? -1 : 1]; };
struct NumFrames {
	char framesnotcorrectlydefined[LENGTH(geometry) == NFRAMES ? -1 : 1];
	char atleastonetiledframerequired[NFRAMES < 1 ? -1 : 1];
	char numberofframestoohigh[NFRAMES > 32 ? -1 : 1 ]; };
/*}}}*/
/* Debugging test message {{{*/
#ifdef USETESTMESSAGE
char test[64];
int testpos = 0;
#define TESTMESSAGE(X) snprintf(test, (sizeof test + (testpos = 0)), (X));
void testpush(char ch)
{
	if (testpos >= (sizeof test) - 2)
		testpos = 0;
	test[testpos++] = ch;
	test[testpos] = '\0';
	drawbar(selmon);
}
void testclear(void)
{
	testpos = 0;
	test[0] = '\0';
	drawbar(selmon);
}
#endif/*}}}*/
/* Functions {{{*/
/* applyrules() {{{*/
void applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	int tagwasset = 0;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			if (r->isfloating)
				c->isfloating = 1;
			if (r->tag >= 0 && r->tag  < NTAGS) {
				tagwasset = 1;
				c->tag = r->tag;
			}
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (!tagwasset)
		c->tag = (c->mon->frames + c->mon->selfrm)->tag;
}/*}}}*/
/* applysizehints() {{{*/
int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || isfloating(c)) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}/*}}}*/
/* arrange() {{{*/
void arrange(Monitor *m)
{
	if (m) {
		showhide(m->stack);
		arrangemon(m);
		restack(m);
	} else {
		for (m = mons; m; m = m->next)
			showhide(m->stack);
		for (m = mons; m; m = m->next)
			arrangemon(m);
	}
}/*}}}*/
/* arrangemon() {{{*/
void arrangemon(Monitor *m)
{
	unsigned int i;
	Client *c;

	if (!m)
		return;
	for (i = 1; i <= m->nopenfrms; i++){
		Frame * fr = m->frames + i;
		
		if ((c = fr->last) && !isfloating(c))
			resize(fr->last, fr->x, fr->y, fr->w - 2 * c->bw,
					fr->h - 2 * c->bw, 0);
	}
}/*}}}*/
/* attach() {{{*/
void attach(Client *c)
{
	if (!c)
		return;
	c->next = c->mon->clients;
	c->mon->clients = c;
}/*}}}*/
/* attachfocus() {{{*/
void attachfocus(Client * c)
{
	ClientLink * add;

	if (!c || !c->focusto)
		return;
	if (c == c->focusto) {
		c->focusto = 0;
		return;
	}
	add = ecalloc(1, sizeof(ClientLink));
	add->c = c;
	add->next = c->focusto->focusfrom;
	add->prev = 0;
	if (c->focusto->focusfrom)
		c->focusto->focusfrom->prev = add;
	c->focusto->focusfrom = add;
}/*}}}*/
/* attachstack() {{{*/
void attachstack(Client *c)
{
	if (!c)
		return;
	c->snext = c->mon->stack;
	c->sprev = 0;
	if (c->mon->stack)
		c->mon->stack->sprev = c;
	else
		c->mon->stacklast = c;
	c->mon->stack = c;
}/*}}}*/
/* buttonpress() {{{*/
void buttonpress(XEvent *e)
{
	unsigned int i, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(FOCUSED(selmon), 1);
		selmon = m;
		refocus();
	}
	if (ev->window == selmon->barwin) {
		if (ev->x < m->btagx[NTAGS-1]) {
			for (i = 0; i < NTAGS && ev->x >= m->btagx[i]; i++);
			click = ClkTagBar;
			arg.ui = i;
		} else if (ev->x < m->bfrmx[NFRAMES-1]) {
			for (i = 0; i < NFRAMES && ev->x >= m->bfrmx[i]; i++);
			click = ClkFrmBar;
			arg.ui = i;
		} else if (ev->x < m->bseltitlex)
			click = ClkWinTitle;
		else if (ev->x > selmon->ww - TEXTW(stext))
			click = ClkStatusText;
		else if (m->selfrm == m->focfrm)
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focusclient(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	} else {
		int whichfrm = clicktoframe(m, (float)(ev->x - m->wx)/(float)(m->ww),
									   (float)(ev->y - m->wy)/(float)(m->wh));
		if (whichfrm) {
			arg.ui = whichfrm;	
			click = ClkFrm;
		}
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func
			&& buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(
				(click == ClkTagBar || click == ClkFrmBar || ClkFrm)
					&& buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}/*}}}*/
/* checkoverwm() {{{*/
void checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}/*}}}*/
/* cleanup() {{{*/
void cleanup(void)
{
	Arg a = {.ui = ~0};
	Monitor *m;
	size_t i;

	view(&a);
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}/*}}}*/
/* cleanupmon() {{{*/
void cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon->frames);
	free(mon);
}/*}}}*/
/* clicktoframe() {{{*/
int clicktoframe(Monitor * m, float fx, float fy)
{
	if (m->nopenfrms >= 1) {
		int i;
		const float * geo = geometry[m->nopenfrms - 1]; 

		for (i = 0; i < m->nopenfrms; i++) {
			float x = *(geo++); float y = *(geo++);
			float mx = *(geo++) + x; float my = *(geo++) + y;

			if (fx >= x && fy >= y && fx < mx && fy < my )
				return i + 1;
		}
	}
	return 0;
}/*}}}*/
/* clientmessage() {{{*/
void clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */
					&& !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != FOCUSED(selmon) && !c->isurgent)
			seturgent(c, 1);
	}
}/*}}}*/
/* configure() {{{*/
void configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}/*}}}*/
/* configurenotify() {{{*/
void configurenotify(XEvent *e)
{
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next)
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			refocus();
			arrange(NULL);
		}
	}
}/*}}}*/
/* configurerequest() {{{*/
void configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (isfloating(c)) {
			m = c->mon;
			if (ev->value_mask & CWX)
				c->x = m->mx + ev->x;
			if (ev->value_mask & CWY)
				c->y = m->my + ev->y;
			if (ev->value_mask & CWWidth)
				c->w = ev->width;
			if (ev->value_mask & CWHeight)
				c->h = ev->height;
			/* center in x direction */
			if ((c->x + c->w) > m->mx + m->mw)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2);
			/* center in y direction */
			if ((c->y + c->h) > m->my + m->mh)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2);
			if ((ev->value_mask & (CWX|CWY))
					&& !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (isinfrm(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}/*}}}*/
/* createmon() {{{*/
Monitor * createmon(void)
{
	int i;
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	for (i = 0; i < NFRAMES; i++) {
		Frame * fr = m->frames + i;
		/* floating fr set to default so managed clients inherit tag */
		fr->tag = DEFAULTTAG;
		fr->last = NULL;
		fr->mon = m;
	}
	m->nopenfrms = framesonstart > NFRAMES - 1 ? NFRAMES - 1 : framesonstart;
	m->selfrm = m->selfrmold = m->nopenfrms;
	m->focfrm = m->focfrmold = m->nopenfrms;
	m->showbar = showbar;
	m->topbar = topbar;
	return m;
}/*}}}*/
/* destroynotify() {{{*/
void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}/*}}}*/
/* detach() {{{*/
void detach(Client *c)
{
	Client **tc;

	if (!c)
		return;
	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}/*}}}*/
/* detachstack() {{{*/
void detachstack(Client *c)
{
	if (!c)
		return;
	if (c->snext)
		c->snext->sprev = c->sprev;
	else
		c->mon->stacklast = c->sprev;
	if (c->sprev)
		c->sprev->snext = c->snext;
	else
		c->mon->stack = c->snext;
}/*}}}*/
/* dirtomon() {{{*/
Monitor * dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}/*}}}*/
/* drawbar() {{{*/
void drawbar(Monitor *m)
{
	int x, w, sw = 0, i;
	unsigned int urg[NTAGS];
	unsigned int nclients[NTAGS];
	unsigned int furg = 0, fclients = 0;
	
	char buf[256];
	Client *c;
	Frame * fr = m->frames + m->selfrm;


	int seltag = fr->tag;
	int foctag = (m->frames + m->focfrm)->tag;
	Clr * sfoc, * ssel, * snorm;

	if (m->focfrm) {
		snorm = scheme[SchemeNorm];
		sfoc = scheme[SchemeFoc];
		ssel = scheme[SchemeSel];
	} else {
		snorm = scheme[SchemeFlNorm];
		sfoc = scheme[SchemeFlFoc];
		ssel = scheme[SchemeFlSel];
	}
	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_setscheme(drw, snorm);
		sw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
		drw_text(drw, m->ww - sw, 0, sw, bh, 0, stext, 0);
	}
	for (i = 0; i < NTAGS; i++)
		urg[i] = nclients[i] = 0;
	for (c = m->clients; c; c = c->next) {
		if (c->isfloating) {
			fclients++;
			if (c->isurgent)
				furg = 1;
		} else if (c->tag >= 0 && c->tag < NTAGS) {
			nclients[c->tag]++;
			if (c->isurgent)
				urg[c->tag] = 1;
		}
	}
	x = 0;
	/* draw tags */
	for (i = 0; i < NTAGS; i++) {
		snprintf(buf, sizeof buf, "%d", nclients[i]);

		w = TEXTW(buf);
		drw_setscheme(drw,(m->focfrm && foctag == i) ? sfoc :
						  ((m->selfrm && seltag == i) ? ssel : snorm));
		drw_text(drw, x, 0, w, bh, lrpad/2, buf, 0);
		if (urg[i])
			drw_rect(drw, x + 1, 1, w-2, bh-2, 0, 0);
		m->btagx[i] = (x += w);
	}
	/* draw frame information */
	snprintf(buf, sizeof buf, "[%d]", fclients);
	w = TEXTW(buf) - lrpad;
	drw_setscheme(drw, 0 == m->focfrm ? sfoc :(0 == m->selfrm ? ssel : snorm));
	m->bfrmx[0] = x = drw_text(drw, x, 0, w, bh, 0, buf, furg);
	if (furg)
		drw_rect(drw, x - w + 1, 1, w-2, bh-2, 0, 0);
	for (i = 1; i < NFRAMES; i++) {
		c = m->frames[i].last;
		snprintf(buf, sizeof buf, i<=m->nopenfrms ?
								((c && !c->isfloating) ? "[+]":"[-]"):"[ ]");
		w = TEXTW(buf) - lrpad;
		drw_setscheme(drw, i==m->focfrm ? sfoc : (i==m->selfrm ? ssel :snorm));
		m->bfrmx[i] = x = drw_text(drw, x, 0, w, bh, 0, buf, 0);
	}
#ifdef TESTMESSAGE
	if (*test) {
		w = TEXTW(test);
		drw_setscheme(drw, ssel);
		x = drw_text(drw, x, 0, w, bh, lrpad / 2, test, 0);
	}
#endif
	/* draw title */
	if (m->ww - sw - x > bh) {
		if (m->focfrm != m->selfrm && (c = SELECTED(m))) {
			char shortname[17];

			strncpy(shortname, c->name, (sizeof shortname) - 1);
			w = TEXTW(shortname);
			drw_setscheme(drw, m == selmon ? ssel : snorm);
			m->bseltitlex = drw_text(drw, x, 0, w, bh, lrpad/2,shortname,0);
			x = m->bseltitlex;
		} else
			m->bseltitlex = x;
		w = m->ww - sw - x;
		if ((c = FOCUSED(m))) {
			drw_setscheme(drw, m == selmon ? sfoc : snorm);
			drw_text(drw, x, 0, w, bh, lrpad / 2, c->name, 0);
		} else {
			drw_setscheme(drw, snorm);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}/*}}}*/
/* drawbars() {{{*/
void drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}/*}}}*/
/* enternotify() {{{*/
void enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior)
			&& ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(FOCUSED(selmon), 1);
		selmon = m;
		refocus();
	} else if (c == FOCUSED(selmon)) {
		return;
	}
}/*}}}*/
/* exchangeframecontents() {{{*/
void exchangeframecontents(unsigned int a, unsigned int b)
{
	Frame * fr = selmon->frames;
	Client * c;
	int i;
	if (a == b || a >= NFRAMES || b >= NFRAMES || a == 0 || b == 0)
		return;
	setselfrm(selmon, (a == selmon->selfrm ? b :
				(b == selmon->selfrm ? a : selmon->selfrm)));
	setfocfrm(selmon, (a == selmon->focfrm ? b :
				(b == selmon->focfrm ? a : selmon->focfrm)));
	c = fr[a].last;
	fr[a].last = fr[b].last;
	fr[b].last = c;
	i = fr[a].tag;
	fr[a].tag = fr[b].tag;
	fr[b].tag = i;
}/*}}}*/
/* expose() {{{*/
void expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}/*}}}*/
/* focusclient() {{{*/
void focusclient(Client *c)
{
/* should this be focusing the already focused client? */
	int i;

	if (c) {
		Frame * fr = c->mon->frames;

		unfocus(FOCUSED(selmon), 0);
		if (isfloating(c)) {
			selmon = c->mon;
			setselfrm(selmon, 0);
			setfocfrm(selmon, 0);
			FOCUSED(selmon) = c;
		} else {
			for (i = 1; i <= c->mon->nopenfrms && c != fr[i].last; i++);
			if (i <= c->mon->nopenfrms) {
				selmon = c->mon;
				setfocfrm(selmon, i);
				setselfrm(selmon, i);
				FOCUSED(selmon) = c;
			}
		}
	}
	if (FOCUSED(selmon))
		refocus();
	else
		focusnothing();
	arrange(selmon);
}/*}}}*/
/* focusin() {{{*/
/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;
	Client * c = FOCUSED(selmon);

	if (c && ev->window != c->win)
		refocus();
}/*}}}*/
/* focusnothing() {{{*/
void focusnothing()
{
	/* focuses the bar instead of actually focusing nothing */
	XSetInputFocus(dpy, selmon->barwin, RevertToPointerRoot, CurrentTime);
	XChangeProperty(dpy, root, netatom[NetActiveWindow],
		XA_WINDOW, 32, PropModeReplace,
		(unsigned char *) &(selmon->barwin), 1);
	/* this is what was replaced from dwm */
	/* XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]); */
}/*}}}*/
/* getatomprop {{{*/
Atom getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}/*}}}*/
/* getrootptr() {{{*/
int getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}/*}}}*/
/* getstate() {{{*/
long getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}/*}}}*/
/* gettextprop() {{{*/
int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}/*}}}*/
/* grabbuttons() {{{*/
void grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] =
							{ 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused) /* doesn't produce every event on focused window */
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}/*}}}*/
/* grabkeys() {{{*/
void grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}/*}}}*/
/* isavailable() {{{*/
int isavailable(Client * c)
{
	Monitor * m = c->mon;
	Frame * fr = m->frames + m->selfrm;
	if (m->selfrm) {
		return (c->tag == fr->tag) && !isinfrm(c);
	}else{ /* any except focused client is available for the floating frame */
		return c->isfloating && c != fr->last;
	}
}/*}}}*/
/* isfloating() {{{*/
int isfloating(Client * c)
{
	if (!c)
		return 0;
	return  c->isfixed || c->isfloating;
}/*}}}*/
/* isinfrm() {{{*/
int isinfrm(Client * c)
{
	int i;
	Frame * fr;
	
	if (!c)
		return 0;
	if (c->isfloating) /* floating windows */
		return 1;
	fr = c->mon->frames;
	if (c->mon->nopenfrms) /* windows in frames */
		for (i = 1; i <= c->mon->nopenfrms; i++)
			if (fr[i].last == c)
				return 1;
	return 0;
}/*}}}*/
/* isuniquegeom() {{{*/
#ifdef XINERAMA
int isuniquegeom(XineramaScreenInfo *unique, size_t n,
						XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */
/*}}}*/
/* keypress() {{{*/
void keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}/*}}}*/
/* consider breaking this into smaller functions */
/* manage() {{{*/
void manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->isfixed = c->isurgent = c->neverfocus = 0;
	c->isfloating = c->isfullscreen = 0;
	c->lastfrm = 0;
	c->focusto = 0;
	c->focusfrom = 0;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tag = t->tag;
		c->focusto = t;
	} else {
		c->mon = selmon;
		applyrules(c);
	}
	if (c->mon->selfrm == 0) {
		c->isfloating = 1;
	}
	if (trans != None)
		c->isfloating = 1;
	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y,
			((c->mon->by == c->mon->my) &&
			 (c->x + (c->w / 2) >= c->mon->wx) &&
			 (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	updatewindowtype(c); /* sets floating, so needs to happen before border */
	attachfocus(c);
	c->bw = c->isfloating ? flborderpx : borderpx;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask
							|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	/* frame filling uses client list so add after the selected */
	if ((t = SELECTED(c->mon))) {
		c->next = t->next;
		t->next = c;
	} else {
		attach(c);
	}
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
					PropModeAppend, (unsigned char *) &(c->win), 1);
	/* some windows require this */
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	XMapWindow(dpy, c->win);
	if (c->mon == selmon && selmon->selfrm == selmon->focfrm) {
		unfocus(FOCUSED(selmon), 0);
	}
	if (c->isfloating) {
		SELECTED(c->mon) = selwinforselfrm(c->mon, 0);
		if (c->mon->selfrm == c->mon->focfrm) {
			setfocfrm(c->mon, 0);
			setselfrm(c->mon, 0);
		} else {
			setselfrm(c->mon, 0);
			restack(selmon);
		}
		c->mon->frames->last = c;
	} else {
		SELECTED(c->mon) = c;
	}
	if (c->mon == selmon && selmon->selfrm == selmon->focfrm)
		refocus();
	arrange(c->mon);
}/*}}}*/
/* mappingnotify() {{{*/
void mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}/*}}}*/
/* maprequest() {{{*/
void maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}/*}}}*/
/* moitionnotify() {{{*/
void motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(FOCUSED(selmon), 1);
		selmon = m;
		refocus();
	}
	mon = m;
}/*}}}*/
/* propertynotify() {{{*/
void propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!isfloating(c) && (XGetTransientForHint(dpy, c->win, &trans))&&
				(wintoclient(trans) != NULL)) {
				c->isfloating = 1;
				arrange(c->mon);
			}
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == FOCUSED(c->mon))
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}/*}}}*/
/* recttomon() {{{*/
Monitor * recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}/*}}}*/
/* refocus() {{{*/
void refocus(void)
{
	Client * c = FOCUSED(selmon);

	if (c) {
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		SETBORDERCOL(c);
		setfocus(c);
	} else {
		focusnothing();
	}
	drawbars();
}/*}}}*/
/* resize() {{{*/
void resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}/*}}}*/
/* resizeclient() {{{*/
void resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->x = wc.x = x;
	c->y = wc.y = y;
	c->w = wc.width = w;
	c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}/*}}}*/
/* restack() {{{*/
void restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	wc.stack_mode = Below;
	wc.sibling = m->barwin;
	for (c = m->stack; c; c = c->snext)
		if (!isfloating(c) && isinfrm(c)) {
			XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
			wc.sibling = c->win;
		}
	wc.sibling = m->barwin;
	wc.stack_mode = Above;
	for (c = m->stacklast; c; c = c->sprev)
		if (isfloating(c)) {
			XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
			wc.sibling = c->win;
		}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}/*}}}*/
/* restacksel() {{{*/
void restacksel(void) 
{
	Client * s = SELECTED(selmon), * f = FOCUSED(selmon);

	detachstack(s);
	attachstack(s);
	detachstack(f);
	attachstack(f);
}/*}}}*/
/* run() {{{*/
void run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type]) {
			handler[ev.type](&ev); /* call handler */
			/*
			switch (ev.type) {
			case ButtonPress: testpush('0'); break;
			case ClientMessage: testpush('1'); break;
			case ConfigureRequest: testpush('2'); break;
			case ConfigureNotify: testpush('3'); break;
			case DestroyNotify: testpush('4'); break;
			case EnterNotify: testpush('5'); break;
			case Expose: testpush('6'); break;
			case FocusIn: testpush('7'); break;
			case KeyPress: testpush('8'); break;
			case MappingNotify: testpush('9'); break;
			case MapRequest: testpush('a'); break;
			case MotionNotify: testpush('b'); break;
			case PropertyNotify: testpush('c'); break;
			case UnmapNotify: testpush('d'); break;
			}
			*/
		}
}/*}}}*/
/* scan() {{{*/
void scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}/*}}}*/
/* sendevent(){{{*/
int sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}/*}}}*/
/* sendmon() {{{*/
void sendmon(Client *c, Monitor *m)
{
	int i;

	if (!c || !m || c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tag = (m->frames + m->selfrm)->tag; /* assign tags of selected frame */
	attach(c);
	attachstack(c);
	if (c == selmon->frames[selmon->selfrm].last) { /* refill selected */
		selmon->frames[selmon->selfrm].last = selwinforselfrm(selmon, 1);
		if (selmon->selfrm == selmon->focfrm)
			refocus();
	}
	for (i = 0; i < NFRAMES; i++) /* clear others */
		if (c == selmon->frames[i].last)
			selmon->frames[i].last = NULL;
	arrange(NULL);
}/*}}}*/
/* selwinforselfrm() {{{*/
Client * selwinforselfrm(Monitor * m, int dir)
{
	Frame * fr = m->frames + m->selfrm;
	Client * i, * c = NULL;

	if (dir == 0) /* try to refill the frame, with most recent if not last*/
	{
		c = fr->last;
		fr->last = NULL;
		for (; c && !isavailable(c); c = c->snext);
		if (!c)
			for (c = m->stack; c && !isavailable(c); c = c->snext);
	}else if (dir > 0) { /* next in client list*/
		c = fr->last;
		for (; c && !isavailable(c); c = c->next);
		if (!c)
			for (c = m->clients; c && !isavailable(c); c = c->next);
	} else { /* previous in client list*/
		for (i = m->clients; i && i != fr->last; i = i->next)
			if (isavailable(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (isavailable(i))
					c = i;
	}
	return c;
}/*}}}*/
/* setclientstate() {{{*/
void setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}/*}}}*/
/* setfocfrm() {{{*/
void setfocfrm(Monitor * m, unsigned int frm)
{
	if (m->focfrm != frm) {
		m->focfrmold = m->focfrm;
		m->focfrm = frm;
	}
}/*}}}*/
/* setfocus() {{{*/
void setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}/*}}}*/
/* setfullscreen() {{{*/
void setfullscreen(Client *c, int fullscreen)
{
	/* after a few attempts to make fullscreen window behavior consistent and
	 * bug free, I realized I have no idea what the correct behavior for
	 * fullscreen windows should be in x */
	/* making a window fill the screen is already possible other ways, so
	 * instead I decided to have setting fullscreen do nothing, so you can use
	 * a program's fullscreen mode in any size window */
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		configure(c); /* fixes some windows rendering content past edges */
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
	}
}/*}}}*/
/* setopenframes() {{{*/
static void setopenframes(Monitor * m, unsigned int nf)
{
	int i, j;
	Arg arg;

	if (nf > NFRAMES)
		return;
	for (i = selmon->nopenfrms + 1; i < NFRAMES; i++)
		for (j = 1; j <= selmon->nopenfrms; j++)
			if (selmon->frames[i].last == selmon->frames[j].last) {
				selmon->frames[i].last = NULL;
				break;
			}
	m->nopenfrms = nf;
	if (nf < nframehooks) {
		switch(nf) {
			case 1: arg.v = framehooks[1]; break;
			case 2: arg.v = framehooks[2]; break;
			case 3: arg.v = framehooks[3]; break;
			default: arg.v = framehooks[0];
		}
		spawn(&arg);
	}
}
/*}}}*/
/* setselfrm() {{{*/
void setselfrm(Monitor * m, unsigned int frm)
{
	if (m->selfrm != frm) {
		m->selfrmold = m->selfrm;
		m->selfrm = frm;
	}
}/*}}}*/
/* setup() {{{*/
void setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	nframehooks = LENGTH(framehooks);
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "staticdwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	refocus();
}/*}}}*/
/* seturgent() {{{*/
void seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}/*}}}*/
/* showhide() {{{*/
void showhide(Client *c)
{
	if (!c)
		return;
	if (isinfrm(c)) { 
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if (isfloating(c))
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}/*}}}*/
/* sigchld() {{{*/
void sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}/*}}}*/
/* unfocus() {{{*/
void unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus)
		focusnothing();
}/*}}}*/
/* unmanage() {{{*/
void unmanage(Client *c, int destroyed)
{
	int i;
	Client * focusto;
	Monitor * m;
	XWindowChanges wc;

	if (!c)
		return;
	m = c->mon;
	focusto = c->focusto;
	while (c->focusfrom) { /* clear focusto references to this client */
		ClientLink * todel = c->focusfrom;

		c->focusfrom->c->focusto = focusto;
		attachfocus(c->focusfrom->c); /* focus c's focusto instead of c*/
		c->focusfrom = c->focusfrom->next;
		free(todel);
	}
	c->focusfrom = focusto ? focusto->focusfrom : 0;
	while (c->focusfrom) { /* clear focusfrom reference to this client */
		if (c->focusfrom->c == c) {
			if (c->focusfrom->prev)
				c->focusfrom->prev->next = c->focusfrom->next;
			else
				focusto->focusfrom = c->focusfrom->next;
			if (c->focusfrom->next)
				c->focusfrom->next->prev = c->focusfrom->prev;
			free(c->focusfrom);
			break;
		}
		c->focusfrom = c->focusfrom->next;
	}
	if (c == m->frames[m->selfrm].last) /* refill selected */
		m->frames[m->selfrm].last = selwinforselfrm(m, -1);
	for (i = 0; i < NFRAMES; i++) /* clear others */
		if (c == selmon->frames[i].last)
			selmon->frames[i].last = NULL;
	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	updateclientlist();
	if (m == selmon) {
		if (focusto)
			focusclient(focusto);
		else
			refocus();
	}
	arrange(m);
}/*}}}*/
/* unmapnotify() {{{*/
void unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}/*}}}*/
/* updatebars() {{{*/
void updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"staticdwm", "staticdwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0,
				DefaultDepth(dpy, screen), CopyFromParent,
				DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}/*}}}*/
/* updatebarpos() {{{*/
void updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
}/*}}}*/
/* updateborder() {{{*/
void updateborder(Client * c, int bw)
{
	XWindowChanges wc;

	if (!c || bw < 0)
		return;
	wc.border_width = c->bw = bw;
	XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
	/*configure(c);*/
}/*}}}*/
/* updateclientlist() {{{*/
void updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}/*}}}*/
/* updatefrmpos() {{{*/
void updatefrmpos(Monitor * m)
{
	if (m->nopenfrms <= sizeof geometry && m->nopenfrms > 0) {
		int i;
		Frame * fr = m->frames;
		const float * geo = geometry[m->nopenfrms - 1];
		int y = m->wy, h = m->wh;

		fr->x = m->wx;
		fr->y = y;
		fr->w = m->ww;
		fr->h = h;
		fr++;
		for (i = 1; i <= m->nopenfrms; i++, fr++)
		{
			fr->x = (*(geo++))*m->ww + m->wx;
			fr->y = (*(geo++))*h + y;
			fr->w = (*(geo++))*m->ww;
			fr->h = (*(geo++))*h;
		}
	}
}/*}}}*/
/* updategeom() {{{*/
int updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
					updatefrmpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
			updatefrmpos(mons);
		}
	}
	if (dirty) {
		selmon = mons; /* wintomon may return selmon */
		selmon = wintomon(root);
	}
	return dirty;
}/*}}}*/
/* updatenumlockmask() {{{*/
void updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}/*}}}*/
/* updatesizehints() {{{*/
void updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}/*}}}*/
/* updatetitle() {{{*/
void updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}/*}}}*/
/* updatestatus() {{{*/
void updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "staticdwm-"VERSION);
	drawbar(selmon);
}/*}}}*/
/* updatewindowtype() {{{*/
void updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog]) {
		c->isfloating = 1;
		if (!c->focusto)
			c->focusto = FOCUSED(selmon);
	}
}/*}}}*/
/* updatewmhints() {{{*/
void updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == FOCUSED(selmon) && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}/*}}}*/
/* wintoclient() {{{*/
Client * wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}/*}}}*/
/* wintomon() {{{*/
Monitor * wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}/*}}}*/
/* xerror() {{{*/
/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "staticdwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}/*}}}*/
/* xerrordummy() {{{*/
int xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}/*}}}*/
/* xerrorstart() {{{*/
/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("staticdwm: another window manager is already running");
	return -1;
}/*}}}*/
/* main() {{{*/
int main(int argc, char *argv[])
{
	Monitor * m;

	if (argc == 2 && !strcmp("-v", argv[1]))
		die("staticdwm-"VERSION);
	else if (argc != 1)
		die("usage: staticdwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("staticdwm: cannot open display");
	checkotherwm();
	setup();
	scan();
	updatefrmpos(selmon);
	arrange(selmon);
	for (m = mons; m; m = m->next) /* call frame swap hooks on start */
		setopenframes(m, m->nopenfrms);
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}/*}}}*/
/*}}}*/
/* User Called Functions {{{*/
/* closeframe(){{{*/
void closeframe(const Arg *arg)
{
	int i;
	unsigned int foc, sel = selmon->selfrmold;

	if (selmon->selfrm == 0)
		return;
	if (sel > selmon->selfrm) sel--;/*try selold after removing sel*/
	if (sel >= selmon->nopenfrms) sel = selmon->selfrm;/*or what is now sel*/
	if (sel == selmon->nopenfrms) sel--;/*or if sel was last, the new last*/
	if (selmon->selfrm == selmon->focfrm) {
		foc = sel;
		unfocus(FOCUSED(selmon), 0);
	} else {
		foc = selmon->focfrm; /* focus should not move if set differently */
		if (foc >= selmon->selfrm) foc--; /* except because sel was removed */
	}
	for (i = selmon->selfrm; i < selmon->nopenfrms; i++)
		exchangeframecontents(i, i+1);
	setfocfrm(selmon, foc);
	setselfrm(selmon, sel);
	setopenframes(selmon, selmon->nopenfrms - 1);
	if (foc == sel)
		refocus();
	updatefrmpos(selmon);
	arrange(selmon);
}/*}}}*/
/* closeframebelow(){{{*/
void closeframebelow(const Arg *arg)
{
	if (!arg)
		return;
	if (arg->ui > selmon->nopenfrms) {
		if (arg->ui < NFRAMES) {
			setopenframes(selmon, arg->ui);
			updatefrmpos(selmon);
			arrange(selmon);
		}
		return;
	}
	setopenframes(selmon, arg->ui);
	if (selmon->selfrm > arg->ui)
		setselfrm(selmon, arg->ui);
	if (selmon->focfrm > arg->ui) {
		unfocus(FOCUSED(selmon), 0);
		setfocfrm(selmon, arg->ui);
		refocus();
	}
	updatefrmpos(selmon);
	arrange(selmon);
}/*}}}*/
/* emptyframe() {{{*/
void emptyframe(const Arg *arg)
{
	if (selmon->selfrm == selmon->focfrm)
		unfocus(FOCUSED(selmon), 0);
	selmon->frames[selmon->selfrm].last = NULL;
	if (selmon->selfrm == selmon->focfrm)
		focusnothing();
	arrange(selmon);
}/*}}}*/
/* fillframe() {{{*/
void fillframe(const Arg *arg)
{
	Client *c;

	if (arg && (c = selwinforselfrm(selmon, arg->i))) {
		if (selmon->selfrm == selmon->focfrm)
			unfocus(FOCUSED(selmon), 0);
		selmon->frames[selmon->selfrm].last = c;
		if (selmon->selfrm == selmon->focfrm)
			refocus();
		else
			restacksel();
		arrange(selmon);
	}
}/*}}}*/
/* focusframe() {{{*/
void focusframe(const Arg *arg)
{
	if (arg && arg->ui < NFRAMES &&
		(selmon->focfrm != arg->ui || selmon->selfrm != arg->ui)) {
		unfocus(FOCUSED(selmon), 0);
		setfocfrm(selmon, arg->ui);
		setselfrm(selmon, arg->ui);
		if (selmon->nopenfrms < arg->ui) {
			setopenframes(selmon, arg->ui);
			updatefrmpos(selmon);
		}
		if (FOCUSED(selmon))
			refocus();
		else
			focusnothing();
		arrange(selmon);
	}
}/*}}}*/
/* focusmon() {{{*/
void focusmon(const Arg *arg)
{
	Monitor *m;

	if (!arg || !mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(FOCUSED(selmon), 0);
	selmon = m;
	refocus();
}/*}}}*/
/* killclient() {{{*/
static void killclient(const Arg *arg)
{
	Client * c = SELECTED(selmon);

	if (c && !sendevent(c, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, c->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}/*}}}*/
/* movemouse() {{{*/
void movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = FOCUSED(selmon)))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!isfloating(c) && (abs(nx - c->x) > snap
							   || abs(ny - c->y) > snap)) {
				c->oldx = nx; c->oldy = ny; c->oldw = c->w; c->oldh = c->h;
				togglefloating(NULL);
			}
			if (isfloating(c))
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		refocus();
	}
}/*}}}*/
/* onlyframe(){{{*/
void onlyframe(const Arg *arg)
{
	if (selmon->selfrm <= 0)
		return;
	if (selmon->focfrm != 1)
		unfocus(FOCUSED(selmon), 0);
	if (selmon->selfrm != 1)
		exchangeframecontents(1, selmon->selfrm);
	setfocfrm(selmon, 1);
	setselfrm(selmon, 1);
	refocus();
	setopenframes(selmon, 1);
	updatefrmpos(selmon);
	arrange(selmon);
}/*}}}*/
/* quit() {{{*/
void quit(const Arg *arg)
{
	running = 0;
}/*}}}*/
/* resizemouse() {{{*/
void resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = FOCUSED(selmon)))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
					c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx
				&& c->mon->wx + nw <= selmon->wx + selmon->ww
				&& c->mon->wy + nh >= selmon->wy
				&& c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!isfloating(c) && (abs(nw - c->w) > snap
								   || abs(nh - c->h) > snap)) {
					c->oldx = c->x; c->oldy = c->y; c->oldw = nw; c->oldh = nh;
					togglefloating(NULL);
				}
			}
			if (isfloating(c))
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
					c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		refocus();
	}
}/*}}}*/
/* selectframe(){{{*/
void selectframe(const Arg *arg)
{
	if (arg && arg->ui < NFRAMES && selmon->selfrm != arg->ui) {
		setselfrm(selmon, arg->ui);
		if (selmon->nopenfrms < arg->ui) {
			setopenframes(selmon, arg->ui);
			updatefrmpos(selmon);
		}
		arrange(selmon);
		drawbar(selmon); /* update frame selection marker */
	}
}/*}}}*/
/* spawn() {{{*/
void spawn(const Arg *arg)
{
	if (!arg)
		return;
	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "staticdwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}/*}}}*/
/* swapfocus() {{{*/
void swapfocus(const Arg *arg)
{
	int self, focf;
	if (selmon->nopenfrms == 0)
		return;
	if (selmon->selfrm != selmon->focfrm) {
		focf = selmon->selfrm;
		self = selmon->focfrm;
	} else if (((focf = selmon->focfrmold) != selmon->focfrm ||
			    (focf = selmon->selfrmold) != selmon->focfrm) &&
				 focf <= selmon->nopenfrms) {
		self = focf;
	} else { /* prefer swapping to first over second over floating */
		focf = self = selmon->focfrm!=1 ? 1 : (selmon->nopenfrms>1 ? 2 : 0);
	}
	unfocus(FOCUSED(selmon), 0);
	setselfrm(selmon, self);
	setfocfrm(selmon, focf);
	refocus();
	restack(selmon);
}/*}}}*/
/* swapframe() {{{*/
void swapframe(const Arg *arg)
{
	int f;

	if (selmon->focfrm == 0 || selmon->selfrm == 0 || selmon->nopenfrms < 2)
		return; /* don't swap to floating */
	if (selmon->selfrm != selmon->focfrm) {
		f = selmon->selfrm;
		exchangeframecontents(selmon->selfrm, selmon->focfrm);
	} else if ((((f = selmon->focfrmold) != selmon->focfrm && f != 0) ||
			    ((f = selmon->selfrmold) != selmon->focfrm && f != 0)) &&
				 f <= selmon->nopenfrms) {
	} else { /* prefer swapping with first, or second if first is focused */
		f = selmon->focfrm == 1 ? 2 : 1;
	}
	exchangeframecontents(f, selmon->focfrm);
	arrange(selmon);
}/*}}}*/
/* tag() {{{*/
void tag(const Arg *arg)
{
	Frame * fr = selmon->frames + selmon->selfrm;
	Client * c = fr->last;
	if (!arg || !c || arg->ui >= NTAGS)
		return;
	c->tag = arg->ui;
	if (c->isfloating) {
		c->isfloating = 0;
		if (selmon->selfrm == selmon->focfrm) {
			updateborder(c, borderpx);
			SETBORDERCOL(c);
			unfocus(c, 0);
			c = selmon->frames->last = selwinforselfrm(selmon, 0);
			focusclient(c);
		}
		arrange(selmon);
	} else {
		if (selmon->selfrm == selmon->focfrm)
			unfocus(fr->last, 0);
		fr->last = selwinforselfrm(selmon, 0);
		if (selmon->selfrm == selmon->focfrm)
			refocus();
		else
			restacksel();
		arrange(selmon);
	}
}/*}}}*/
/* tagandview() {{{*/
void tagandview(const Arg *arg)
{
	Frame * fr = selmon->frames + selmon->selfrm;
	if (!arg || arg->ui >= NTAGS || !fr->last ||
		fr->last->isfloating)
		return;
	fr->last->tag = arg->ui;
	fr->tag = arg->ui;
	drawbars();
}/*}}}*/
/* tagmon() {{{*/
void tagmon(const Arg *arg)
{
	if (!arg || !SELECTED(selmon) || !mons->next)
		return;
	sendmon(SELECTED(selmon), dirtomon(arg->i));
}/*}}}*/
/* togglebar() {{{*/
void togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	updatefrmpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by,
						selmon->ww, bh);
	arrange(selmon);
} /*}}}*/
/* togglefloating() {{{*/
void togglefloating(const Arg *arg)
{
	Client * c = SELECTED(selmon);
	Frame * fr = selmon->frames + selmon->selfrm;

	if (!c)
		return;
	if (!isfloating(c)) {
		c->isfloating = 1;
		c->bw = flborderpx;
		resizeclient(c, c->oldx, c->oldy, c->oldw, c->oldh);
		c->lastfrm = selmon->selfrm;
		fr->last = selwinforselfrm(selmon, 0);
		if (selmon->selfrm == selmon->focfrm) {
			setfocfrm(selmon, 0);
			setselfrm(selmon, 0);
		} else {
			setselfrm(selmon, 0);
			restack(selmon);
		}
		selmon->frames->last = c;
		arrange(selmon);
	} else {
		c->isfloating = 0;
		updateborder(c, borderpx);
		c->oldx = c->x; c->oldy = c->y; c->oldw = c->w; c->oldh = c->h;
		if (c->lastfrm > 0 && c->lastfrm <= selmon->nopenfrms &&
				selmon->frames[c->lastfrm].tag == c->tag) {
			if (selmon->selfrm == selmon->focfrm) {
				setfocfrm(selmon, c->lastfrm);
				setselfrm(selmon, c->lastfrm);
				SELECTED(selmon) = c;
			} else {
				setselfrm(selmon, c->lastfrm);
				if (selmon->focfrm != selmon->selfrm)
					SELECTED(selmon) = c; /* don't fill focused frame */
			}
		} else if (selmon->selfrm == selmon->focfrm) {
			unfocus(c, 0);
			focusclient((selmon->frames->last = selwinforselfrm(selmon, 0)));
		}
		arrange(selmon);
	}
	SETBORDERCOL(c);
} /*}}}*/
/* toggleframe() {{{*/
void toggleframe(const Arg *arg)
{
	int sel = selmon->selfrm;

	if (!arg)
		return;
	if (arg->i <= 0)
		sel = (sel == 0 || sel == selmon->nopenfrms) ? 1 : sel + 1;
	else
		sel = (sel == 0 || sel == 1) ? selmon->nopenfrms : sel - 1;
	if (selmon->selfrm != sel) {
		unfocus(FOCUSED(selmon), 0);
		setfocfrm(selmon, sel);
		setselfrm(selmon, sel);
		/*refill tiled frame previously filled with floating client*/
		if (selmon->selfrm && (isfloating(FOCUSED(selmon))))
			FOCUSED(selmon) = NULL;
		refocus();
		arrange(selmon);
	}
}/*}}}*/
/* view() {{{*/
void view(const Arg *arg)
{
	unsigned int tag = arg->ui;
	Frame * fr = selmon->frames + selmon->selfrm;

	if (selmon->selfrm == 0 || tag < 0 ||
			tag >= NTAGS || (tag == fr->tag && SELECTED(selmon)))
		return;
	if (selmon->selfrm == selmon->focfrm)
		unfocus(fr->last, 0);
	fr->tag = tag;
	fr->last = selwinforselfrm(selmon, 0);
	if (selmon->selfrm == selmon->focfrm)
		refocus();
	else 
		restacksel();
	if (!FOCUSED(selmon))
		focusnothing();
	arrange(selmon);
}/*}}}*/
/*}}}*/
