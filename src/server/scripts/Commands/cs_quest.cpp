/*
 * Copyright (C) 2008-2010 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: quest_commandscript
%Complete: 100
Comment: All quest related commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "ObjectMgr.h"
#include "Chat.h"

class quest_commandscript : public CommandScript
{
public:
    quest_commandscript() : CommandScript("quest_commandscript") { }

    ChatCommand* GetCommands() const
    {
        static ChatCommand questCommandTable[] =
        {
            { "add",            SEC_ADMINISTRATOR,  false, &HandleQuestAdd,                    "", NULL },
            { "complete",       SEC_ADMINISTRATOR,  false, &HandleQuestComplete,               "", NULL },
            { "remove",         SEC_ADMINISTRATOR,  false, &HandleQuestRemove,                 "", NULL },
            { NULL,             0,                  false, NULL,                               "", NULL }
        };
        static ChatCommand commandTable[] =
        {
            { "quest",          SEC_ADMINISTRATOR,  false, NULL,                  "", questCommandTable },
            { NULL,             0,                  false, NULL,                               "", NULL }
        };
        return commandTable;
    }

    static bool HandleQuestAdd(ChatHandler* handler, const char* args)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target->NoModify() && !handler->GetSession()->GetPlayer()->IsAdmin() && handler->GetSession()->GetPlayer() != target)
        {
            handler->PSendSysMessage("This player has disabled GM commands being used on them.");
            return true;
        }

        // .addquest #entry'
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args,"Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* pQuest = sObjectMgr->GetQuestTemplate(entry);

        if (!pQuest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND,entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // check item starting quest (it can work incorrectly if added without item in inventory)
        for (uint32 id = 0; id < sItemStorage.MaxEntry; id++)
        {
            ItemPrototype const *pProto = sItemStorage.LookupEntry<ItemPrototype>(id);
            if (!pProto)
                continue;

            if (pProto->StartQuest == entry)
            {
                handler->PSendSysMessage(LANG_COMMAND_QUEST_STARTFROMITEM, entry, pProto->ItemId);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // ok, normal (creature/GO starting) quest
        if (target->CanAddQuest(pQuest, true))
        {
            target->AddQuest(pQuest, NULL);

            if (target->CanCompleteQuest(entry))
                target->CompleteQuest(entry);
        }

        return true;
    }

    static bool HandleQuestRemove(ChatHandler* handler, const char* args)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->NoModify() && !handler->GetSession()->GetPlayer()->IsAdmin() && handler->GetSession()->GetPlayer() != player)
        {
            handler->PSendSysMessage("This player has disabled GM commands being used on them.");
            return true;
        }

        // .removequest #entry'
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args,"Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* pQuest = sObjectMgr->GetQuestTemplate(entry);

        if (!pQuest)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // remove all quest entries for 'entry' from quest log
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 quest = player->GetQuestSlotQuestId(slot);
            if (quest == entry)
            {
                player->SetQuestSlot(slot,0);

                // we ignore unequippable quest items in this case, its' still be equipped
                player->TakeQuestSourceItem(quest, false);
            }
        }

        player->RemoveActiveQuest(entry);
        player->RemoveRewardedQuest(entry);

        handler->SendSysMessage(LANG_COMMAND_QUEST_REMOVED);
        return true;
    }

    static bool HandleQuestComplete(ChatHandler* handler, const char* args)
    {
        Player* target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (target->NoModify() && !handler->GetSession()->GetPlayer()->IsAdmin() && handler->GetSession()->GetPlayer() != target)
        {
            handler->PSendSysMessage("This player has disabled GM commands being used on them.");
            return true;
        }

        // .quest complete #entry
        // number or [name] Shift-click form |color|Hquest:quest_id:quest_level|h[name]|h|r
        char* cId = handler->extractKeyFromLink((char*)args,"Hquest");
        if (!cId)
            return false;

        uint32 entry = atol(cId);

        Quest const* pQuest = sObjectMgr->GetQuestTemplate(entry);

        // If player doesn't have the quest
        if (!pQuest || target->GetQuestStatus(entry) == QUEST_STATUS_NONE)
        {
            handler->PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Add quest items for quests that require items
        for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
        {
            uint32 id = pQuest->ReqItemId[x];
            uint32 count = pQuest->ReqItemCount[x];
            if (!id || !count)
                continue;

            uint32 curItemCount = target->GetItemCount(id,true);

            ItemPosCountVec dest;
            uint8 msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, count-curItemCount);
            if (msg == EQUIP_ERR_OK)
            {
                Item* item = target->StoreNewItem(dest, id, true);
                target->SendNewItem(item,count-curItemCount,true,false);
            }
        }

        // All creature/GO slain/casted (not required, but otherwise it will display "Creature slain 0/10")
        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            int32 creature = pQuest->ReqCreatureOrGOId[i];
            uint32 creaturecount = pQuest->ReqCreatureOrGOCount[i];

            if (uint32 spell_id = pQuest->ReqSpell[i])
            {
                for (uint16 z = 0; z < creaturecount; ++z)
                    target->CastedCreatureOrGO(creature,0,spell_id);
            }
            else if (creature > 0)
            {
                if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(creature))
                    for (uint16 z = 0; z < creaturecount; ++z)
                        target->KilledMonster(cInfo,0);
            }
            else if (creature < 0)
            {
                for (uint16 z = 0; z < creaturecount; ++z)
                    target->CastedCreatureOrGO(creature,0,0);
            }
        }

        // If the quest requires reputation to complete
        if (uint32 repFaction = pQuest->GetRepObjectiveFaction())
        {
            uint32 repValue = pQuest->GetRepObjectiveValue();
            uint32 curRep = target->GetReputationMgr().GetReputation(repFaction);
            if (curRep < repValue)
                if (FactionEntry const *factionEntry = sFactionStore.LookupEntry(repFaction))
                    target->GetReputationMgr().SetReputation(factionEntry,repValue);
        }

        // If the quest requires a SECOND reputation to complete
        if (uint32 repFaction = pQuest->GetRepObjectiveFaction2())
        {
            uint32 repValue2 = pQuest->GetRepObjectiveValue2();
            uint32 curRep = target->GetReputationMgr().GetReputation(repFaction);
            if (curRep < repValue2)
                if (FactionEntry const *factionEntry = sFactionStore.LookupEntry(repFaction))
                    target->GetReputationMgr().SetReputation(factionEntry,repValue2);
        }

        // If the quest requires money
        int32 ReqOrRewMoney = pQuest->GetRewOrReqMoney();
        if (ReqOrRewMoney < 0)
            target->ModifyMoney(-ReqOrRewMoney);

        target->CompleteQuest(entry);
        return true;
    }
};

void AddSC_quest_commandscript()
{
    new quest_commandscript();
}
