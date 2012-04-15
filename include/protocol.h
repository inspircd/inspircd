/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

class User;

class ProtoServer
{
 public:
	std::string servername;
	std::string parentname;
	std::string gecos;
	unsigned int usercount;
	unsigned int latencyms;
};

typedef std::list<ProtoServer> ProtoServerList;

class SyncTarget : public classbase
{
 public:
	/**
	 * Send metadata during the netburst
	 *
	 * @param target The Channel* or User* that metadata should be sent for. NULL for network metadata.
	 * @param extname The extension name to send metadata for
	 * @param extdata Encoded data for this extension
	 */
	virtual void SendMetaData(Extensible* target, const std::string &extname, const std::string &extdata) = 0;
	/**
	 * Send an ENCAP * command during netburst (for network sync)
	 * @param cmd The subcommand to send (the receiver should have a handler for this)
	 * @param params The parameters to the command; only the last can contain spaces
	 */
	virtual void SendEncap(const std::string& cmd, const parameterlist &params) = 0;
	/**
	 * Send a line s2s during netburst. The line should begin with the command you
	 * are sending; ":SID " will be prepended prior to sending.
	 */
	virtual void SendCommand(const std::string &line) = 0;
};

class ProtocolInterface
{
 public:
	ProtocolInterface() { }
	virtual ~ProtocolInterface() { }

	/** Send an ENCAP message to one or more linked servers.
	 * See the protocol documentation for the purpose of ENCAP.
	 * @param encap This is a list of string parameters, the first of which must be a server ID or glob matching servernames.
	 * The second must be a subcommand. All subsequent parameters are dependant on the subcommand.
	 * ENCAP (should) be used instead of creating new protocol messages for easier third party application support.
	 * @return True if the message was sent out (target exists)
	 */
	virtual bool SendEncapsulatedData(const parameterlist &encap) { return false; }

	/** Send metadata for an object to other linked servers.
	 * @param target The object to send metadata for.
	 * @param key The 'key' of the data, e.g. "swhois" for swhois desc on a user
	 * @param data The string representation of the data
	 */
	virtual void SendMetaData(Extensible* target, const std::string &key, const std::string &data) { }

	/** Send a topic change for a channel
	 * @param channel The channel to change the topic for.
	 * @param topic The new topic to use for the channel.
	 */
	virtual void SendTopic(Channel* channel, std::string &topic) { }

	/** Send mode changes for an object.
	 * @param target The channel name or user to send mode changes for.
	 * @param The mode changes to send.
	 * @param merge Whether or not to send the modes as a merge.
	 */
	virtual void SendMode(User *src, Extensible* target, irc::modestacker& modes, bool merge = false) { }

	/** Send a notice to users with a given snomask.
	 * @param snomask The snomask required for the message to be sent.
	 * @param text The message to send.
	 */
	virtual void SendSNONotice(const std::string &snomask, const std::string &text) { }

	/** Send raw data to a remote client.
	 * @param target The user to push data to.
	 * @param rawline The raw IRC protocol line to deliver (":me NOTICE you :foo", whatever).
	 */
	virtual void PushToClient(User* target, const std::string &rawline) { }

	/** Send a message to a channel.
	 * @param target The channel to message.
	 * @param status The status character (e.g. %) required to recieve.
	 * @param text The message to send.
	 */
	virtual void SendChannelPrivmsg(Channel* target, char status, const std::string &text) { }

	/** Send a notice to a channel.
	 * @param target The channel to message.
	 * @param status The status character (e.g. %) required to recieve.
	 * @param text The message to send.
	 */
	virtual void SendChannelNotice(Channel* target, char status, const std::string &text) { }

	/** Send a message to a user.
	 * @param target The user to message.
	 * @param text The message to send.
	 */
	virtual void SendUserPrivmsg(User* target, const std::string &text) { }

	/** Send a notice to a user.
	 * @param target The user to message.
	 * @param text The message to send.
	 */
	virtual void SendUserNotice(User* target, const std::string &text) { }

	/** Fill a list of servers and information about them.
	 * @param sl The list of servers to fill.
	 * XXX: document me properly, this is shit.
	 */
	virtual void GetServerList(ProtoServerList &sl) { }
};

#endif

