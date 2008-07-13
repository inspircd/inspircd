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
	RPL_WELCOME						=	001,
	RPL_YOURHOSTIS					=	002,
	RPL_SERVERCREATED				=	003,
	RPL_SERVERVERSION				=	004,

	RPL_YOURUUID					=	042, // taken from ircnet

	RPL_TOPIC						=	332,
	RPL_TOPICTIME					=	333,
	RPL_NAMREPLY					=	353,
	RPL_ENDOFNAMES					=	366,


	/*
	 * Error range of numerics.
	 */
	ERR_NOSUCHNICK					=	401,
	ERR_TOOMANYCHANNELS				=	405,
	ERR_USERNOTINCHANNEL			=	441,
	ERR_BADCHANNELKEY				=	475,
	ERR_INVITEONLYCHAN				=	473,
	ERR_CHANNELISFULL				=	471,
	ERR_BANNEDFROMCHAN				=	474,
	ERR_CHANOPRIVSNEEDED			=	482,

	ERR_UNKNOWNSNOMASK				=	501
};
