away-notify client capability specification
----------------------------------------------

Copyright (c) 2012 Keith Buck <mr_flea@esper.net>.

Unlimited redistribution and modification of this document is allowed
provided that the above copyright notice and this permission notice
remains in tact.

The away-notify client capability allows a client to specify that it
would like to be notified when users are marked/unmarked as away. This
capability is referred to as 'away-notify' at capability negotiation
time.

This capability is designed to replace polling of WHO as a more
efficient method of tracking the away state of users in a channel. The
away-notify capability both conserves bandwidth as WHO requests are
not continually sent and allows the client to be notified immediately
upon a user setting or removing their away state (as opposed to when
WHO is next polled).

When this capability is enabled, clients will be sent an AWAY message
when a user sharing a channel with them sets or removes their away
state, as well as when a user joins and has an away message set.
(Note that AWAY will not be sent for joining users with no away
message set.)

The format of the AWAY message is as follows:

    :nick!user@host AWAY [:message]

If the message is present, the user (specified by the nick!user@host
mask) is going away.  If the message is not present, the user is
removing their away message/state.

To fully track the away state of users, clients should:

1) Enable the away-notify capability at negotiation time.

2) Execute WHO when joining a channel to capture the current away
   state of all users in that channel.

3) Update state appropriately upon receiving an AWAY message.
