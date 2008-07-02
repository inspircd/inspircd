/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/** Right, Line Buffers. Time for an explanation as to how sendqs work. By the way, ircd people,
 * before you jump up and down screaming, this is not anything like Adrian Chadd's linebuffer
 * stuff done for hybrid(and never really completed.) Rather, this is (I think) where linebuffer
 * was heading, and should have been.
 *
 * Enough introduction, actual explanation starts here.
 * In IRCd, we have a sendq. Traditionally, a sendq has been a big string. Stuff is tacked onto
 * the end, and when we can, we send the user some data off the start of it. A circular buffer.
 *
 * This model works okay, and is quite simplistic to code, but has a drawback in that most IRC
 * messages are multicast: topic changes, joins, parts, channel messages, and so on.
 * This means that for each of these messages, we must do O(n^n) amount of work: bytes ^ recipients
 * of writes will be made to sendqs, and this is a slow and expensive operation.
 *
 * The solution comes with the use of this linebuffer class below, which is managed entirely by
 * the user class (though it *may* be possible to add a server to server implementation of this later,
 * but that's nowhere near so needed, and nowhere near so trivial, thanks to the inherited nature
 * of buffered socket, but I digress).
 *
 * What this class does, in a nutshell:
 * When we need to send a message to a user, we create a LineBuffer object. It has a reference count, and
 * we copy the string we need to send into the LineBuffer object also.
 * We then tack a pointer to this LineBuffer into an std::list stored in the User class.
 * When the user writes data, a ptr is advanced depending how much of that line they wrote. If they wrote all
 * of the line,  the pointer is popped off the std::list, the ptr is reset, and the buffer's refcount is
 * decremented - and if it reaches 0, the linebuffer is destroyed as it has fulfilled it's purpose.
 *
 * Effectively, this means that multicast writes become O(n) + time taken to copy message once, or just about.
 *
 * We gain efficiency, and much, much better RAM usage.
 */
static unsigned int totalbuffers = 0;

class LineBuffer
{
 private:
	std::string msg;
	unsigned long refcount;

	// Don't let it be copied.
	LineBuffer(const LineBuffer &) { }

 public:
	~LineBuffer()
	{
		totalbuffers--;
		printf("Destroying LineBuffer with %u bytes, total buffers is %u\n", msg.length(), totalbuffers);
		msg.resize(0);
	}

	LineBuffer(std::string &m)
	{
		if (m.length() > MAXBUF - 2) /* MAXBUF has a value of 514, to account for line terminators */
		{
			// Trim the message to fit, 510 characters max.
			m = m.substr(0, MAXBUF - 4); // MAXBUF is 514, we need 510.
		}

		// Add line terminator
		m.append("\r\n");
		
		// And copy
		msg = m;
		refcount = 0;
		totalbuffers++;
		printf("Creating LineBuffer with %u bytes, total buffers is %u\n", msg.length(), totalbuffers);
	}

	std::string &GetMessage()
	{
		return msg;
	}

	unsigned long GetMessageLength()
	{
		return msg.length();
	}

	// To be used after creation, when we know how many recipients we actually have.
	void SetRefcount(unsigned long r)
	{
		refcount = r;
	}

	unsigned long DecrementCount()
	{
		if (refcount == 0)
		{
			throw "decrementing a refcount when nobody is using it is weird and wrong";
		}

		refcount--;
		return refcount;
	}

	// There is no increment method as it isn't really needed.
};
