/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 0;        /* border pixel of windows */
static const unsigned int flborderpx= 3;		/* floating win border pix */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=10"};
static const char dmenufont[]       =  "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan2[]       = "#005577";
static const char col_cyan1[]       = "#227799";
static const char col_mag2[]        = "#770c00";
static const char col_mag1[]        = "#992e22";

static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan2, col_cyan1 },
	[SchemeFoc]  = { col_gray4, col_cyan1, col_cyan2 },
	[SchemeFlNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeFlSel]  = { col_gray4, col_mag2, col_mag1 },
	[SchemeFlFoc]  = { col_gray4, col_mag1, col_mag2 },
};

/* frame geometry */
#define MAXTILEDFRAMES 3
static const float one_frame[] =	{0.0f, 0.0f, 1.0f, 1.0f};
static const float two_frames[] =	{0.0f, 0.0f, 0.5f, 1.0f,
									 0.5f, 0.0f, 0.5f, 1.0f};
static const float three_frames[] = {0.0f, 0.0f, 0.5f, 1.0f,
									 0.5f, 0.5f, 0.5f, 0.5f,
									 0.5f, 0.0f, 0.5f, 0.5f};
static const float *geometry[] = {one_frame, two_frames, three_frames};

/* tagging */
#define NTAGS 4
#define DEFAULTTAG 0

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* set tag as -1 to not specify */
	/* class      instance    title       tag     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       -1,            0,          -1 },
	{ "Firefox",  NULL,		  NULL,		   2,			 0,			 -1 },
};

/* layout(s) */
static const int resizehints = 1;/*1 means respect size hints in tiled resize*/
static const int framesonstart = 1;

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = TAG} }, \
	{ MODKEY|ControlMask,          	KEY,      tagandview,     {.ui = TAG} }, \
	{ MODKEY|ShiftMask,           	KEY,      tag,     		  {.ui = TAG} },
#define FRAMEKEYS(KEY,FRAME) \
	{ MODKEY,                       KEY,      focusframe,     {.ui = FRAME} }, \
	{ MODKEY|ControlMask,           KEY,      closeframebelow,{.ui = FRAME} }, \
	{ MODKEY|ShiftMask,             KEY,      selectframe,    {.ui = FRAME} },

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] =
	{"dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf",
		col_gray3, "-sb", col_cyan1, "-sf", col_gray4, NULL };
static const char *termcmd[] = { "st", NULL };


/* commands called when frames are opened or closed, first command when no
 * frames are left open, second when one, etc. MUST define as NULL for no hooks.*/
static const char **framehooks[] = {NULL, NULL, NULL, NULL};

/* Below is an example use which loads a different background for each layout.
static const char *bgcmd[] = {"feh","--bg-scale","/home/michael/bg.jpg",NULL};
static const char *bg1cmd[] = {"feh","--bg-scale","/home/michael/bg1.jpg",NULL};
static const char *bg2cmd[] = {"feh","--bg-scale","/home/michael/bg2.jpg",NULL};
static const char *bg3cmd[] = {"feh","--bg-scale","/home/michael/bg3.jpg",NULL};
static const char **framehooks[] = {bgcmd, bg1cmd, bg2cmd, bg3cmd};
*/

/* available functions: closeframe, closeframebelow, emptyframe, fillframe,
 * focusframe, focusmon, killclient, movemouse, onlyframe, quit, resizemouse,
 * selectframe, spawn, swapfocus, swapframe, tag, tagmon, togglebar,
 * togglefloating, toggleframe, toggletag, toggleview, view */
static Key keys[] = {
	/* modifier                    key        function		  argument */
	{MODKEY,                       XK_p,      spawn,          {.v = dmenucmd}},
	{MODKEY,		               XK_Return, spawn,          {.v = termcmd}},
	{MODKEY,                       XK_b,      togglebar,      {0}},
	{MODKEY,                       XK_j,      fillframe,      {.i = -1}},
	{MODKEY,                       XK_k,      fillframe,      {.i = +1}},
	{MODKEY,                       XK_l,      toggleframe,    {.i = -1}},
	{MODKEY,                       XK_h,      toggleframe,    {.i = +1}},
	{MODKEY,                       XK_r,      closeframe,	  {0}},
	{MODKEY,                       XK_x,      swapframe,	  {0}},
	{MODKEY,		               XK_space,  swapfocus,      {0}},
	{MODKEY,                       XK_o,      onlyframe,	  {0}},
	{MODKEY|ShiftMask,             XK_c,      killclient,     {0}},
	{MODKEY|ShiftMask,             XK_space,  togglefloating, {0}},
	{MODKEY,                       XK_comma,  focusmon,       {.i = -1}},
	{MODKEY,                       XK_period, focusmon,       {.i = +1}},
	{MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1}},
	{MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1}},
	{MODKEY,		               XK_minus,  emptyframe,     {0}},
	{MODKEY|ShiftMask,             XK_q,      quit,           {0}},
	FRAMEKEYS(                     XK_a,                      0)
	FRAMEKEYS(                     XK_s,                      1)
	FRAMEKEYS(                     XK_d,                      2)
	FRAMEKEYS(                     XK_f,                      3)
	TAGKEYS(                       XK_1,                      0)
	TAGKEYS(                       XK_2,                      1)
	TAGKEYS(                       XK_3,                      2)
	TAGKEYS(                       XK_4,                      3)
};

/* button definitions. ClkWinTitle will only affect selected window, title, not
 * the focused window title, if it is different*/
/* click can be ClkTagbar, ClkFrmBar, ClkStatusText, ClkWinTitle,
 * ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating,	{0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkWinTitle, 			0, 				Button1,		fillframe,		{.i = 1} },
	{ ClkWinTitle, 			MODKEY,			Button1,		fillframe,		{.i = -1} },
	{ ClkWinTitle,          0,         		Button2,        killclient,		{0} },
	{ ClkWinTitle,          0,         		Button3,        swapfocus,		{0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,			    Button2,        tag,            {0} },
	{ ClkTagBar,            0,			    Button3,        tagandview,     {0} },
	{ ClkFrmBar,            0,	            Button1,        focusframe,     {0} },
	{ ClkFrmBar,            0,	            Button2,        closeframebelow,{0} },
	{ ClkFrmBar,            0,	            Button3,        selectframe,    {0} },
	{ ClkFrm,        	    0,	            Button1,        focusframe,     {0} },
};
