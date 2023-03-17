/* BotServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/* Indices for TTB (Times To Ban) */
enum {
    TTB_BOLDS,
    TTB_COLORS,
    TTB_REVERSES,
    TTB_UNDERLINES,
    TTB_BADWORDS,
    TTB_CAPS,
    TTB_FLOOD,
    TTB_REPEAT,
    TTB_ITALICS,
    TTB_AMSGS,
    TTB_SIZE
};

struct KickerData {
    bool amsgs, badwords, bolds, caps, colors, flood, italics, repeat, reverses,
         underlines;
    int16_t ttb[TTB_SIZE];                    /* Times to ban for each kicker */
    int16_t capsmin, capspercent;             /* For CAPS kicker */
    int16_t floodlines, floodsecs;            /* For FLOOD kicker */
    int16_t repeattimes;                      /* For REPEAT kicker */

    bool dontkickops, dontkickvoices;

  protected:
    KickerData() { }

  public:
    virtual ~KickerData() { }
    virtual void Check(ChannelInfo *ci) = 0;
};
