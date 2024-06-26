/*
 * YurOTS, a free game server emulator 
 * Official Repository on Github <https://github.com/RafaelTolomeotti/yurOTS-Tibinha>
 * Copyright (C) 2024 - RafaelTolomeotti <https://github.com/RafaelTolomeotti>
 * A fork of The Forgotten Server(Mark Samman) branch 1.2 and part of Nostalrius(Alejandro Mujica) repositories.
 *
 * The MIT License (MIT). Copyright © 2020 <YurOTS>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#ifndef FS_PROTOCOLGAME_H_FACA2A2D1A9348B78E8FD7E8003EBB87
#define FS_PROTOCOLGAME_H_FACA2A2D1A9348B78E8FD7E8003EBB87

#include "protocol.h"
#include "chat.h"
#include "creature.h"
#include "tasks.h"

class NetworkMessage;
class Player;
class Game;
class House;
class Container;
class Tile;
class Connection;
class Quest;
class ProtocolGame;
typedef std::shared_ptr<ProtocolGame> ProtocolGame_ptr;

extern Game g_game;

struct TextMessage
{
	MessageClasses type;
	std::string text;
	TextMessage() = default;
	TextMessage(MessageClasses type, std::string text) : type(type), text(std::move(text)) {}
};

class ProtocolGame final : public Protocol
{
	public:
		// static protocol information
		enum {server_sends_first = false};
		enum {protocol_identifier = 0x0A}; // Not required as we send first
		
		static const char* protocol_name() {
			return "gameworld protocol";
		}

		explicit ProtocolGame(Connection_ptr connection) : Protocol(connection) {}

		void login(const std::string& name, uint32_t accnumber, OperatingSystem_t operatingSystem);
		void logout(bool displayEffect, bool forced);

		uint16_t getVersion() const {
			return version;
		}

	private:
		ProtocolGame_ptr getThis() {
			return std::static_pointer_cast<ProtocolGame>(shared_from_this());
		}
		void connect(uint32_t playerId, OperatingSystem_t operatingSystem);
		void sendUpdateRequest();
		void disconnectClient(const std::string& message) const;
		void writeToOutputBuffer(const NetworkMessage& msg);

		void release() final;

		void checkCreatureAsKnown(uint32_t id, bool& known, uint32_t& removedKnown);

		bool canSee(int32_t x, int32_t y, int32_t z) const;
		bool canSee(const Creature*) const;
		bool canSee(const Position& pos) const;

		// we have all the parse methods
		void parsePacket(NetworkMessage& msg) final;
		void onRecvFirstMessage(NetworkMessage& msg) final;

		//Parse methods
		void parseAutoWalk(NetworkMessage& msg);
		void parseSetOutfit(NetworkMessage& msg);
		void parseSay(NetworkMessage& msg);
		void parseLookAt(NetworkMessage& msg);
		void parseLookInBattleList(NetworkMessage& msg);
		void parseFightModes(NetworkMessage& msg);
		void parseAttack(NetworkMessage& msg);
		void parseFollow(NetworkMessage& msg);

		void parseProcessRuleViolationReport(NetworkMessage& msg);
		void parseCloseRuleViolationReport(NetworkMessage& msg);

		void parseBugReport(NetworkMessage& msg);
		void parseDebugAssert(NetworkMessage& msg);

		void parseThrow(NetworkMessage& msg);
		void parseUseItemEx(NetworkMessage& msg);
		void parseUseWithCreature(NetworkMessage& msg);
		void parseUseItem(NetworkMessage& msg);
		void parseCloseContainer(NetworkMessage& msg);
		void parseUpArrowContainer(NetworkMessage& msg);
		void parseUpdateContainer(NetworkMessage& msg);
		void parseTextWindow(NetworkMessage& msg);
		void parseHouseWindow(NetworkMessage& msg);

		void parseInviteToParty(NetworkMessage& msg);
		void parseJoinParty(NetworkMessage& msg);
		void parseRevokePartyInvite(NetworkMessage& msg);
		void parsePassPartyLeadership(NetworkMessage& msg);

		void parseSeekInContainer(NetworkMessage& msg);

		//trade methods
		void parseRequestTrade(NetworkMessage& msg);
		void parseLookInTrade(NetworkMessage& msg);

		//VIP methods
		void parseAddVip(NetworkMessage& msg);
		void parseRemoveVip(NetworkMessage& msg);

		void parseRotateItem(NetworkMessage& msg);

		//Channel tabs
		void parseChannelInvite(NetworkMessage& msg);
		void parseChannelExclude(NetworkMessage& msg);
		void parseOpenChannel(NetworkMessage& msg);
		void parseOpenPrivateChannel(NetworkMessage& msg);
		void parseCloseChannel(NetworkMessage& msg);
		void parseEnableSharedPartyExperience(NetworkMessage& msg);

		//Send functions
		void sendClosePrivate(uint16_t channelId);
		void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName);
		void sendChannelsDialog();
		void sendChannel(uint16_t channelId, const std::string& channelName);
		void sendOpenPrivateChannel(const std::string& receiver);
		void sendToChannel(const Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId);
		void sendPrivateMessage(const Player* speaker, SpeakClasses type, const std::string& text);
		void sendIcons(uint16_t icons);

		void sendDistanceShoot(const Position& from, const Position& to, uint8_t type);
		void sendMagicEffect(const Position& pos, uint8_t type);
		void sendCreatureHealth(const Creature* creature);
		void sendSkills();
		void sendPing();
		void sendPingBack();
		void sendCreatureTurn(const Creature* creature, uint32_t stackpos);
		void sendCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text, const Position* pos = nullptr);

		void sendCancelWalk();
		void sendChangeSpeed(const Creature* creature, uint32_t speed);
		void sendCancelTarget();
		void sendCreatureOutfit(const Creature* creature, const Outfit_t& outfit);
		void sendStats();
		void sendTextMessage(const TextMessage& message);
		void sendAnimatedText(const Position& pos, uint8_t color, const std::string& text);

		void sendCreatureShield(const Creature* creature);
		void sendCreatureSkull(const Creature* creature);

		void sendTradeItemRequest(const std::string& traderName, const Item* item, bool ack);
		void sendCloseTrade();

		void sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxlen, bool canWrite);
		void sendTextWindow(uint32_t windowTextId, uint32_t itemId, const std::string& text);
		void sendHouseWindow(uint32_t windowTextId, const std::string& text);
		void sendOutfitWindow();

		void sendUpdatedVIPStatus(uint32_t guid, VipStatus_t newStatus);
		void sendVIP(uint32_t guid, const std::string& name, VipStatus_t status);
		void sendVIPEntries();

		void sendFightModes();

		void sendCreatureLight(const Creature* creature);
		void sendWorldLight(const LightInfo& lightInfo);

		void sendCreatureSquare(const Creature* creature, SquareColor_t color);

		//rule violations
		void sendRemoveRuleViolationReport(const std::string& name);
		void sendLockRuleViolation();
		void sendRuleViolationCancel(const std::string& name);
		void sendRuleViolationsChannel(uint16_t channelId);

		//tiles
		void sendMapDescription(const Position& pos);

		void sendAddTileItem(const Position& pos, const Item* item, uint32_t stackpos);
		void sendUpdateTileItem(const Position& pos, uint32_t stackpos, const Item* item);
		void sendRemoveTileThing(const Position& pos, uint32_t stackpos);
		void sendUpdateTile(const Tile* tile, const Position& pos);

		void sendAddCreature(const Creature* creature, const Position& pos, int32_t stackpos, bool isLogin);
		void sendMoveCreature(const Creature* creature, const Position& newPos, int32_t newStackPos,
		                      const Position& oldPos, int32_t oldStackPos, bool teleport);

		//containers
		void sendAddContainerItem(uint8_t cid, const Item* item);
		void sendUpdateContainerItem(uint8_t cid, uint16_t slot, const Item* item);
		void sendRemoveContainerItem(uint8_t cid, uint16_t slot);

		void sendContainer(uint8_t cid, const Container* container, bool hasParent, uint16_t firstIndex);
		void sendCloseContainer(uint8_t cid);

		//inventory
		void sendInventoryItem(slots_t slot, const Item* item);

		//Help functions

		// translate a tile to clientreadable format
		void GetTileDescription(const Tile* tile, NetworkMessage& msg);

		// translate a floor to clientreadable format
		void GetFloorDescription(NetworkMessage& msg, int32_t x, int32_t y, int32_t z,
		                         int32_t width, int32_t height, int32_t offset, int32_t& skip);

		// translate a map area to clientreadable format
		void GetMapDescription(int32_t x, int32_t y, int32_t z,
		                       int32_t width, int32_t height, NetworkMessage& msg);

		void AddCreature(NetworkMessage& msg, const Creature* creature, bool known, uint32_t remove);
		void AddPlayerStats(NetworkMessage& msg);
		void AddOutfit(NetworkMessage& msg, const Outfit_t& outfit);
		void AddPlayerSkills(NetworkMessage& msg);
		void AddWorldLight(NetworkMessage& msg, const LightInfo& lightInfo);
		void AddCreatureLight(NetworkMessage& msg, const Creature* creature);
		void AddCreatureSpeak(NetworkMessage& msg, const Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId, const Position* pos = nullptr);

		//tiles
		static void RemoveTileThing(NetworkMessage& msg, const Position& pos, uint32_t stackpos);

		void MoveUpCreature(NetworkMessage& msg, const Creature* creature, const Position& newPos, const Position& oldPos);
		void MoveDownCreature(NetworkMessage& msg, const Creature* creature, const Position& newPos, const Position& oldPos);

		//otclient
		void parseExtendedOpcode(NetworkMessage& msg);

		friend class Player;

		// Helpers so we don't need to bind every time
		template <typename Callable, typename... Args>
		void addGameTask(Callable function, Args&&... args) {
			g_dispatcher.addTask(createTask(std::bind(function, &g_game, std::forward<Args>(args)...)));
		}

		template <typename Callable, typename... Args>
		void addGameTaskTimed(uint32_t delay, Callable function, Args&&... args) {
			g_dispatcher.addTask(createTask(delay, std::bind(function, &g_game, std::forward<Args>(args)...)));
		}

		std::unordered_set<uint32_t> knownCreatureSet;
		Player* player = nullptr;

		uint32_t eventConnect = 0;
		uint16_t version = CLIENT_VERSION_MIN;

		bool debugAssertSent = false;
		bool acceptPackets = false;
};

#endif
