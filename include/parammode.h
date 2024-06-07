/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

class CoreExport ParamModeBase
	: public ModeHandler
{
private:
	virtual void OnUnsetInternal(User* source, Channel* chan) = 0;

public:
	ParamModeBase(Module* Creator, const std::string& Name, char modeletter, ParamSpec ps)
		: ModeHandler(Creator, Name, modeletter, ps, MODETYPE_CHANNEL, MC_PARAM) { }

	/** @copydoc ModeHandler::OnModeChange */
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;

	// Does nothing by default
	virtual bool IsParameterSecret() { return false; }
	virtual void OnUnset(User* source, Channel* chan) { }
	virtual bool OnSet(User* source, Channel* chan, std::string& param) = 0;
	virtual void GetParameter(Channel* chan, std::string& out) = 0;
};

/** Defines a parameter mode
 * T = Child class
 * ExtItemT = Type of the extension item used to store the parameter
 *
 * When unsetting the mode, the extension is automatically unset.
 */
template <typename T, typename ExtItemT>
class ParamMode
	: public ParamModeBase
{
public:
	ExtItemT ext;

	/**
	 * @param Creator Module handling this mode
	 * @param Name The internal name of this mode
	 * @param modeletter The mode letter of this mode
	 * @param ps The parameter type of this mode, one of ParamSpec
	 */
	ParamMode(Module* Creator, const std::string& Name, char modeletter, ParamSpec ps = PARAM_SETONLY)
		: ParamModeBase(Creator, Name, modeletter, ps)
		, ext(Creator, "param-mode-" + Name, ExtensionType::CHANNEL)
	{
	}

	void OnUnsetInternal(User* source, Channel* chan) override
	{
		this->OnUnset(source, chan);
		ext.Unset(chan);
	}

	void GetParameter(Channel* chan, std::string& out) override
	{
		auto mh = static_cast<T*>(this);
		mh->SerializeParam(chan, ext.Get(chan), out);
	}
};
