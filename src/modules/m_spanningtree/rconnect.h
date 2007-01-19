#ifndef __RCONNECT_H__
#define __RCONNECT_H__

/** Handle /RCONNECT
 */
class cmd_rconnect : public command_t
{
        Module* Creator;
        SpanningTreeUtilities* Utils;
 public:
        cmd_rconnect (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const char** parameters, int pcnt, userrec *user);
};

#endif
