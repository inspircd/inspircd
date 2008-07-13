/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/*
 * This file is aimed providing a string that is easier to use than using the numeric
 * directly.
 *
 * Thanks to Darom, jackmcbarn and Brain for suggesting and discussing this.
 *
 * Please note that the list may not be exhaustive, it'll be done when I have
 * nothing better to do with my time. -- w00t (jul 13, 2008)
 */

enum Numerics
{
	/*
	 * Reply range of numerics.
	 */
	RPL_WELCOME						=	001, // not RFC, extremely common though
	RPL_YOURHOSTIS					=	002, // not RFC, extremely common though
	RPL_SERVERCREATED				=	003, // not RFC, extremely common though
	RPL_SERVERVERSION				=	004, // not RFC, extremely common though
	RPL_ISUPPORT					=	005, // not RFC, extremely common though

	RPL_SNOMASKIS					=	008, // unrealircd

	RPL_YOURUUID					=	042, // taken from ircnet

	RPL_UMODEIS						=	221,
	RPL_RULES						=	232, // unrealircd

	RPL_RULESTART					=	308, // unrealircd
	RPL_RULESEND					=	309, // unrealircd
	RPL_CHANNELMODEIS				=	324,
	RPL_CHANNELCREATED				=	329, // ???
	RPL_TOPIC						=	332,
	RPL_TOPICTIME					=	333, // not RFC, extremely common though
	RPL_NAMREPLY					=	353,
	RPL_ENDOFNAMES					=	366,

	RPL_MOTD						=	372,
	RPL_MOTDSTART					=	375,
	RPL_ENDOFMOTD					=	376,

	RPL_YOURDISPLAYEDHOST			=	396, // from charybdis/etc, common convention

	/*
	 * Error range of numerics.
	 */
	ERR_NOSUCHNICK					=	401,
	ERR_TOOMANYCHANNELS				=	405,
	ERR_UNKNOWNCOMMAND				=	421,
	ERR_NOMOTD						=	422,
	ERR_NORULES						=	434, // unrealircd
	ERR_USERNOTINCHANNEL			=	441,
	ERR_NOTREGISTERED				=	451,
	ERR_NEEDMOREPARAMS				=	461,

	/*
	 * A quick side-rant about the next group of numerics..
	 * There are clients out there that like to assume that just because they don't recieve a numeric
	 * they know, that they have joined the channel.
	 *
	 * If IRC was at all properly standardised, this may even be a semi-acceptable assumption to make,
	 * but that's not the case as we all know, so IT IS NOT ACCEPTABLE. Especially for Insp users, where
	 * differing modules MAY potentially choose to block joins and send NOTICEs or other text to the user
	 * instead!
	 *
	 * tl;dr version:
	 *   DON'T MAKE YOUR CLIENT ASSUME YOU JOINED UNLESS YOU RECIEVE A JOIN WITH YOUR DAMN NICK ON IT.
	 * Thanks.
	 *
	 *  -- A message from the IRC group for coder sanity, and w00t
	 */
	ERR_BADCHANNELKEY				=	475,
	ERR_INVITEONLYCHAN				=	473,
	ERR_CHANNELISFULL				=	471,
	ERR_BANNEDFROMCHAN				=	474,

	ERR_NOPRIVILEGES				=	481, // rfc, beware though, we use this for other things opers may not do also
	ERR_CHANOPRIVSNEEDED			=	482, // rfc, beware though, we use this for other things like trying to kick a uline

	ERR_UNKNOWNSNOMASK				=	501, // not rfc. unrealircd?
	ERR_USERSDONTMATCH				=	502,

	ERR_CANTUNLOADMODULE			=	972, // insp-specific
	RPL_UNLOADEDMODULE				=	973, // insp-specific
	ERR_CANTLOADMODULE				=	974, // insp-specific
	RPL_LOADEDMODULE				=	975 // insp-specific
};
