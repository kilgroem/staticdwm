/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=10" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan2[]        = "#005577";
static const char col_cyan1[]        = "#227799";

static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan2, col_cyan1 },
	[SchemeFoc]  = { col_gray4, col_cyan1, col_cyan2 },
};

/* frame geometry */
static const float one_frame[] =	{0.0f, 0.0f, 1.0f, 1.0f};
static const float two_frames[] =	{0.0f, 0.0f, 0.5f, 1.0f,
									 0.5f, 0.0f, 0.5f, 1.0f};
static const float three_frames[] = {0.0f, 0.0f, 0.5f, 1.0f,
									 0.5f, 0.5f, 0.5f, 0.5f,
									 0.5f, 0.0f, 0.5f, 0.5f};
static const float *geometry[] = {one_frame, two_frames, three_frames};

/* tagging */
static const char *tags[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const int resizehints = 1;/*1 means respect size hints in tiled resize*/

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      tag,     		  {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
#define FRAMEKEYS(KEY,FRAME) \
	{ MODKEY,                       KEY,      focusframe,     {.ui = FRAME} }, \
	{ MODKEY|ControlMask,           KEY,      closeframebelow,{.ui = FRAME} }, \
	{ MODKEY|ShiftMask,             KEY,      selectframe,    {.ui = FRAME} },

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan1, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
/* available functions: closeframe, closeframebelow, emptyframe, fillframe,
 * focusframe, focusmon, killclient, movemouse, onlyframe, quit, resizemouse,
 * selectframe, spawn, swapfocus, swapframe, tag, tagmon, togglebar,
 * togglefloating, toggleframe, toggletag, toggleview, view */
/* the argument for killclient kills either focused (0) or selected (1) */

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY,		                XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      fillframe,      {.i = -1 } },
	{ MODKEY,                       XK_k,      fillframe,      {.i = +1 } },
	{ MODKEY,                       XK_l,      toggleframe,    {.i = -1 } },
	{ MODKEY,                       XK_h,      toggleframe,    {.i = +1 } },
	{ MODKEY,                       XK_r,      closeframe,	   {0} },
	{ MODKEY,                       XK_x,      swapframe,	   {0} },
	{ MODKEY,		                XK_space,  swapfocus,      {0} },
	{ MODKEY,                       XK_o,      onlyframe,	   {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {.i = 0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_v,      view,           {.ui = ~0 } },
	{ MODKEY,		                XK_t,      tag,            {.ui = ~1 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	FRAMEKEYS(                      XK_a,                      0)
	FRAMEKEYS(                      XK_s,                      1)
	FRAMEKEYS(                      XK_d,                      2)
	FRAMEKEYS(                      XK_f,                      3)
	TAGKEYS(                        XK_0,                      0)
	TAGKEYS(                        XK_1,                      1)
	TAGKEYS(                        XK_2,                      2)
	TAGKEYS(                        XK_3,                      3)
	TAGKEYS(                        XK_4,                      4)
	TAGKEYS(                        XK_5,                      5)
	TAGKEYS(                        XK_6,                      6)
	TAGKEYS(                        XK_7,                      7)
	TAGKEYS(                        XK_8,                      8)
	TAGKEYS(                        XK_9,                      9)
	{ MODKEY,		                XK_minus,  emptyframe,     {0} },
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagbar, ClkFrmBar, ClkStatusText, ClkWinTitle,
 * ClkSelWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating,	{0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkSelWinTitle,       0,         		Button1,        swapfocus,		{0} },
	{ ClkWinTitle,          0,         		Button2,        killclient,		{.i = 1} },
	{ ClkSelWinTitle,       0,         		Button2,        killclient,		{.i = 0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            ControlMask,    Button1,        tag,            {0} },
	{ ClkTagBar,            ControlMask,    Button3,        toggletag,      {0} },
	{ ClkFrmBar,            0,	            Button1,        focusframe,     {0} },
	{ ClkFrmBar,            0,	            Button2,        closeframebelow,{0} },
	{ ClkFrmBar,            0,	            Button3,        selectframe,    {0} },
};

