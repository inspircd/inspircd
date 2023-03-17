/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ircv3_batch.h"

class LabeledResponseTag : public ClientProtocol::MessageTagProvider {
  private:
    const Cap::Capability& cap;

  public:
    LocalUser* labeluser;
    std::string label;
    const std::string labeltag;

    LabeledResponseTag(Module* mod, const Cap::Capability& capref)
        : ClientProtocol::MessageTagProvider(mod)
        , cap(capref)
        , labeluser(NULL)
        , labeltag("label") {
    }

    ModResult OnProcessTag(User* user, const std::string& tagname,
                           std::string& tagvalue) CXX11_OVERRIDE {
        if (!irc::equals(tagname, labeltag)) {
            return MOD_RES_PASSTHRU;
        }

        // If the tag is empty or too long then we can't accept it.
        if (tagvalue.empty() || tagvalue.size() > 64) {
            return MOD_RES_DENY;
        }

        // If the user is local then we check whether they have the labeled-response
        // cap enabled. If not then we reject the label tag originating from them.
        LocalUser* lu = IS_LOCAL(user);
        if (lu && !cap.get(lu)) {
            return MOD_RES_DENY;
        }

        // Remote users have their label tag checked by their local server.
        return MOD_RES_ALLOW;
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        // Messages only have a label when being sent to a user that sent one.
        return user == labeluser && tagdata.value == label;
    }
};

class ModuleIRCv3LabeledResponse : public Module {
  private:
    Cap::Capability cap;
    LabeledResponseTag tag;
    IRCv3::Batch::API batchmanager;
    IRCv3::Batch::Batch batch;
    IRCv3::Batch::CapReference batchcap;
    ClientProtocol::EventProvider ackmsgprov;
    ClientProtocol::EventProvider labelmsgprov;
    insp::aligned_storage<ClientProtocol::Message> firstmsg;
    size_t msgcount;

    void FlushFirstMsg(LocalUser* user) {
        // This isn't a side effect but we treat it like one to avoid the logic in OnUserWrite.
        firstmsg->SetSideEffect(true);
        user->Send(labelmsgprov, *firstmsg);
        firstmsg->~Message();
    }

  public:
    ModuleIRCv3LabeledResponse()
        : cap(this, "labeled-response")
        , tag(this, cap)
        , batchmanager(this)
        , batch("labeled-response")
        , batchcap(this)
        , ackmsgprov(this, "ACK")
        , labelmsgprov(this, "labeled")
        , msgcount(0)

    {
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        // We only care about the initial unvalidated OnPreCommand call.
        if (validated || tag.labeluser) {
            return MOD_RES_PASSTHRU;
        }

        // We only care about registered users with the labeled-response and batch caps.
        if (user->registered != REG_ALL || !cap.get(user) || !batchcap.get(user)) {
            return MOD_RES_PASSTHRU;
        }

        const ClientProtocol::TagMap& tagmap = parameters.GetTags();
        const ClientProtocol::TagMap::const_iterator labeltag = tagmap.find(tag.labeltag);
        if (labeltag == tagmap.end()) {
            return MOD_RES_PASSTHRU;
        }

        tag.label = labeltag->second.value;
        tag.labeluser = user;
        return MOD_RES_PASSTHRU;
    }

    void OnPostCommand(Command* command, const CommandBase::Params& parameters,
                       LocalUser* user, CmdResult result, bool loop) CXX11_OVERRIDE {
        // Do nothing if this isn't the last OnPostCommand() run for the command.
        //
        // If a parameter for the command was originally a list and the command handler chose to be executed
        // for each element on the list with synthesized parameters (CommandHandler::LoopCall) then this hook
        // too will run for each element on the list plus once after the whole list has been processed.
        // loop will only be false for the last run.
        if (!loop) {
            OnCommandBlocked(command->name, parameters, user);
        }
    }

    void OnCommandBlocked(const std::string& command,
                          const CommandBase::Params& parameters, LocalUser* user) CXX11_OVERRIDE {
        // If no label was sent we don't have to do anything.
        if (!tag.labeluser) {
            return;
        }

        switch (msgcount) {
        case 0: {
            // There was no response so we send an ACK instead.
            ClientProtocol::Message ackmsg("ACK", ServerInstance->FakeClient);
            ackmsg.AddTag(tag.labeltag, &tag, tag.label);
            ackmsg.SetSideEffect(true);
            tag.labeluser->Send(ackmsgprov, ackmsg);
            break;
        }

        case 1: {
            // There was one response which was cached; send it now.
            firstmsg->AddTag(tag.labeltag, &tag, tag.label);
            FlushFirstMsg(user);
            break;
        }

        default: {
            // There was two or more responses; send an end-of-batch.
            if (batchmanager) {
                // Set end start as side effect so we'll ignore it otherwise it'd end up added into the batch.
                batch.GetBatchEndMessage().SetSideEffect(true);
                batchmanager->End(batch);
            }
            break;
        }
        }

        tag.labeluser = NULL;
        msgcount = 0;
    }

    ModResult OnUserWrite(LocalUser* user,
                          ClientProtocol::Message& msg) CXX11_OVERRIDE {
        // The label user is writing a message to another user.
        if (user != tag.labeluser) {
            return MOD_RES_PASSTHRU;
        }

        // The message is a side effect (e.g. a self-PRIVMSG).
        if (msg.IsSideEffect()) {
            return MOD_RES_PASSTHRU;
        }

        switch (++msgcount) {
        case 1: {
            // First reply message. We can' send it yet because we don't know if there will be more.
            new(firstmsg) ClientProtocol::Message(msg);
            firstmsg->CopyAll();
            return MOD_RES_DENY;
        }

        case 2: {
            // Second reply message. This and all subsequent messages need to go into a batch.
            if (batchmanager) {
                batchmanager->Start(batch);

                // Set batch start as side effect so we'll ignore it otherwise it'd end up added into the batch.
                ClientProtocol::Message& batchstartmsg = batch.GetBatchStartMessage();
                batchstartmsg.SetSideEffect(true);
                batchstartmsg.AddTag(tag.labeltag, &tag, tag.label);

                batch.AddToBatch(*firstmsg);
                batch.AddToBatch(msg);
            }

            // Flush first message which triggers the batch start message
            FlushFirstMsg(user);
            return MOD_RES_PASSTHRU;
        }

        default: {
            // Third or later message. Put it in the batch and send directly.
            if (batchmanager) {
                batch.AddToBatch(msg);
            }
            return MOD_RES_PASSTHRU;
        }
        }
    }

    void Prioritize() CXX11_OVERRIDE {
        Module* alias = ServerInstance->Modules->Find("m_alias.so");
        ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_BEFORE, alias);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides support for the IRCv3 Labeled Response specification.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleIRCv3LabeledResponse)
