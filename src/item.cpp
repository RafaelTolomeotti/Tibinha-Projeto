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

#include "otpch.h"

#include "item.h"
#include "container.h"
#include "teleport.h"
#include "mailbox.h"
#include "house.h"
#include "game.h"
#include "bed.h"

#include "actions.h"
#include "spells.h"

extern Game g_game;
extern Spells* g_spells;
extern Vocations g_vocations;

Items Item::items;

Item* Item::CreateItem(const uint16_t type, uint16_t count /*= 0*/)
{
	Item* newItem = nullptr;

	const ItemType& it = Item::items[type];
	if (it.group == ITEM_GROUP_DEPRECATED) {
		return nullptr;
	}

	if (it.stackable && count == 0) {
		count = 1;
	}

	if (it.id != 0) {
		if (it.isDepot()) {
			newItem = new DepotLocker(type);
		} else if (it.isContainer() || it.isChest()) {
			newItem = new Container(type);
		} else if (it.isTeleport()) {
			newItem = new Teleport(type);
		} else if (it.isMagicField()) {
			newItem = new MagicField(type);
		} else if (it.isDoor()) {
			newItem = new Door(type);
		} else if (it.isMailbox()) {
			newItem = new Mailbox(type);
		} else if (it.isBed()) {
			newItem = new BedItem(type);
		} else {
			newItem = new Item(type, count);
		}

		newItem->incrementReferenceCounter();
	}

	return newItem;
}

Container* Item::CreateItemAsContainer(const uint16_t type, uint16_t size)
{
	const ItemType& it = Item::items[type];
	if (it.id == 0 || it.group == ITEM_GROUP_DEPRECATED || it.stackable || it.useable || it.moveable || it.pickupable || it.isDepot() || it.isSplash() || it.isDoor()) {
		return nullptr;
	}

	Container* newItem = new Container(type, size);
	newItem->incrementReferenceCounter();
	return newItem;
}

Item* Item::CreateItem(PropStream& propStream)
{
	uint16_t id;
	if (!propStream.read<uint16_t>(id)) {
		return nullptr;
	}

	switch (id) {
		case ITEM_FIREFIELD_PVP_FULL:
			id = ITEM_FIREFIELD_PERSISTENT_FULL;
			break;

		case ITEM_FIREFIELD_PVP_MEDIUM:
			id = ITEM_FIREFIELD_PERSISTENT_MEDIUM;
			break;

		case ITEM_FIREFIELD_PVP_SMALL:
			id = ITEM_FIREFIELD_PERSISTENT_SMALL;
			break;

		case ITEM_ENERGYFIELD_PVP:
			id = ITEM_ENERGYFIELD_PERSISTENT;
			break;

		case ITEM_POISONFIELD_PVP:
			id = ITEM_POISONFIELD_PERSISTENT;
			break;

		case ITEM_MAGICWALL:
			id = ITEM_MAGICWALL_PERSISTENT;
			break;

		case ITEM_WILDGROWTH:
			id = ITEM_WILDGROWTH_PERSISTENT;
			break;

		default:
			break;
	}

	return Item::CreateItem(id, 0);
}

Item::Item(const uint16_t type, uint16_t count /*= 0*/) :
	id(type)
{
	const ItemType& it = items[id];

	if (it.isFluidContainer() || it.isSplash()) {
		setFluidType(count);
	} else if (it.stackable) {
		if (count != 0) {
			setItemCount(count);
		} else if (it.charges != 0) {
			setItemCount(it.charges);
		}
	} else if (it.charges != 0) {
		if (count != 0) {
			setCharges(count);
		} else {
			setCharges(it.charges);
		}
	} else if (it.isKey()) {
		setIntAttr(ITEM_ATTRIBUTE_KEYNUMBER, count);
	} else if (it.isWeapon()){
		uint16_t chance = 20;
		if (chance != 0 && uniform_random(1, 100) <= chance) {
			uint16_t subChance = 25;

			if (subChance != 0 && uniform_random(1, 100) <= subChance) {
				uint16_t lifeLeechAmount = uniform_random(1, 20);
				setLifeLeech(lifeLeechAmount);
				//setIntAttr(ITEM_ATTRIBUTE_LIFELEECH, lifeLeechAmount);
			}
			if (subChance != 0 && uniform_random(1, 100) <= subChance) {
				int32_t manaLeechAmount = uniform_random(1, 6);
				setManaLeech(manaLeechAmount);
				//setIntAttr(ITEM_ATTRIBUTE_MANALEECH, manaLeechAmount);
			}
			if (subChance != 0 && uniform_random(1, 100) <= subChance) {
				uint16_t criticalAmount = uniform_random(1, 20);
				setCritical(criticalAmount);
				//setIntAttr(ITEM_ATTRIBUTE_CRITICAL, criticalAmount);
			}
		}
	}

	setDefaultDuration();
}

Item::Item(const Item& i) :
	Thing(), id(i.id), count(i.count), loadedFromMap(i.loadedFromMap)
{
	if (i.attributes) {
		attributes.reset(new ItemAttributes(*i.attributes));
	}
}

Item* Item::clone() const
{
	Item* item = Item::CreateItem(id, count);
	if (attributes) {
		item->attributes.reset(new ItemAttributes(*attributes));
	}
	return item;
}

bool Item::equals(const Item* otherItem) const
{
	if (!otherItem || id != otherItem->id) {
		return false;
	}

	if (!attributes) {
		return !otherItem->attributes;
	}

	const auto& otherAttributes = otherItem->attributes;
	if (!otherAttributes || attributes->attributeBits != otherAttributes->attributeBits) {
		return false;
	}

	const auto& attributeList = attributes->attributes;
	const auto& otherAttributeList = otherAttributes->attributes;
	for (const auto& attribute : attributeList) {
		if (ItemAttributes::isStrAttrType(attribute.type)) {
			for (const auto& otherAttribute : otherAttributeList) {
				if (attribute.type == otherAttribute.type && *attribute.value.string != *otherAttribute.value.string) {
					return false;
				}
			}
		} else {
			for (const auto& otherAttribute : otherAttributeList) {
				if (attribute.type == otherAttribute.type && attribute.value.integer != otherAttribute.value.integer) {
					return false;
				}
			}
		}
	}
	return true;
}

void Item::setDefaultSubtype()
{
	const ItemType& it = items[id];

	setItemCount(1);

	if (it.charges != 0) {
		if (it.stackable) {
			setItemCount(it.charges);
		} else {
			setCharges(it.charges);
		}
	}
}

void Item::onRemoved()
{
	ScriptEnvironment::removeTempItem(this);
}

void Item::setID(uint16_t newid)
{
	const ItemType& prevIt = Item::items[id];
	id = newid;

	const ItemType& it = Item::items[newid];
	uint32_t newDuration = it.decayTime * 1000;

	if (newDuration == 0 && !it.stopTime && it.decayTo < 0) {
		removeAttribute(ITEM_ATTRIBUTE_DECAYSTATE);
		removeAttribute(ITEM_ATTRIBUTE_DURATION);
	}

	removeAttribute(ITEM_ATTRIBUTE_CORPSEOWNER);

	if (newDuration > 0 && (!prevIt.stopTime || !hasAttribute(ITEM_ATTRIBUTE_DURATION))) {
		setDecaying(DECAYING_FALSE);
		setDuration(newDuration);
	}
}

Cylinder* Item::getTopParent()
{
	Cylinder* aux = getParent();
	Cylinder* prevaux = dynamic_cast<Cylinder*>(this);
	if (!aux) {
		return prevaux;
	}

	while (aux->getParent() != nullptr) {
		prevaux = aux;
		aux = aux->getParent();
	}

	if (prevaux) {
		return prevaux;
	}
	return aux;
}

const Cylinder* Item::getTopParent() const
{
	const Cylinder* aux = getParent();
	const Cylinder* prevaux = dynamic_cast<const Cylinder*>(this);
	if (!aux) {
		return prevaux;
	}

	while (aux->getParent() != nullptr) {
		prevaux = aux;
		aux = aux->getParent();
	}

	if (prevaux) {
		return prevaux;
	}
	return aux;
}

Tile* Item::getTile()
{
	Cylinder* cylinder = getTopParent();
	//get root cylinder
	if (cylinder && cylinder->getParent()) {
		cylinder = cylinder->getParent();
	}
	return dynamic_cast<Tile*>(cylinder);
}

const Tile* Item::getTile() const
{
	const Cylinder* cylinder = getTopParent();
	//get root cylinder
	if (cylinder && cylinder->getParent()) {
		cylinder = cylinder->getParent();
	}
	return dynamic_cast<const Tile*>(cylinder);
}

uint16_t Item::getSubType() const
{
	const ItemType& it = items[id];
	if (it.isFluidContainer() || it.isSplash()) {
		return getFluidType();
	} else if (it.stackable) {
		return count;
	} else if (it.charges != 0) {
		return getCharges();
	}
	return count;
}

Player* Item::getHoldingPlayer() const
{
	Cylinder* p = getParent();
	while (p) {
		if (p->getCreature()) {
			return p->getCreature()->getPlayer();
		}

		p = p->getParent();
	}
	return nullptr;
}

void Item::setSubType(uint16_t n)
{
	const ItemType& it = items[id];
	if (it.isFluidContainer() || it.isSplash()) {
		setFluidType(n);
	} else if (it.stackable) {
		setItemCount(n);
	} else if (it.charges != 0) {
		setCharges(n);
	} else {
		setItemCount(n);
	}
}

Attr_ReadValue Item::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch (attr) {
		case ATTR_COUNT:
		case ATTR_RUNE_CHARGES: {
			uint8_t count;
			if (!propStream.read<uint8_t>(count)) {
				return ATTR_READ_ERROR;
			}

			setSubType(count);
			break;
		}

		case ATTR_ACTION_ID: {
			uint16_t actionId;
			if (!propStream.read<uint16_t>(actionId)) {
				return ATTR_READ_ERROR;
			}

			setActionId(actionId);
			break;
		}

		case ATTR_MOVEMENT_ID: {
			uint16_t movementId;
			if (!propStream.read<uint16_t>(movementId)) {
				return ATTR_READ_ERROR;
			}

			setMovementID(movementId);
			break;
		}

		case ATTR_TEXT: {
			std::string text;
			if (!propStream.readString(text)) {
				return ATTR_READ_ERROR;
			}

			setText(text);
			break;
		}

		case ATTR_WRITTENDATE: {
			uint32_t writtenDate;
			if (!propStream.read<uint32_t>(writtenDate)) {
				return ATTR_READ_ERROR;
			}

			setDate(writtenDate);
			break;
		}

		case ATTR_WRITTENBY: {
			std::string writer;
			if (!propStream.readString(writer)) {
				return ATTR_READ_ERROR;
			}

			setWriter(writer);
			break;
		}

		case ATTR_DESC: {
			std::string text;
			if (!propStream.readString(text)) {
				return ATTR_READ_ERROR;
			}

			setSpecialDescription(text);
			break;
		}

		case ATTR_CHARGES: {
			uint16_t charges;
			if (!propStream.read<uint16_t>(charges)) {
				return ATTR_READ_ERROR;
			}

			setSubType(charges);
			break;
		}

		case ATTR_DURATION: {
			int32_t duration;
			if (!propStream.read<int32_t>(duration)) {
				return ATTR_READ_ERROR;
			}

			setDuration(std::max<int32_t>(0, duration));
			break;
		}

		case ATTR_DECAYING_STATE: {
			uint8_t state;
			if (!propStream.read<uint8_t>(state)) {
				return ATTR_READ_ERROR;
			}

			if (state != DECAYING_FALSE) {
				setDecaying(DECAYING_PENDING);
			}
			break;
		}

		case ATTR_NAME: {
			std::string name;
			if (!propStream.readString(name)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr(ITEM_ATTRIBUTE_NAME, name);
			break;
		}

		case ATTR_ARTICLE: {
			std::string article;
			if (!propStream.readString(article)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr(ITEM_ATTRIBUTE_ARTICLE, article);
			break;
		}

		case ATTR_PLURALNAME: {
			std::string pluralName;
			if (!propStream.readString(pluralName)) {
				return ATTR_READ_ERROR;
			}

			setStrAttr(ITEM_ATTRIBUTE_PLURALNAME, pluralName);
			break;
		}

		case ATTR_WEIGHT: {
			uint32_t weight;
			if (!propStream.read<uint32_t>(weight)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_WEIGHT, weight);
			break;
		}

		case ATTR_ATTACK: {
			int32_t attack;
			if (!propStream.read<int32_t>(attack)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_ATTACK, attack);
			break;
		}

		case ATTR_DEFENSE: {
			int32_t defense;
			if (!propStream.read<int32_t>(defense)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_DEFENSE, defense);
			break;
		}

		case ATTR_ARMOR: {
			int32_t armor;
			if (!propStream.read<int32_t>(armor)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_ARMOR, armor);
			break;
		}

		case ATTR_SHOOTRANGE: {
			uint8_t shootRange;
			if (!propStream.read<uint8_t>(shootRange)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_SHOOTRANGE, shootRange);
			break;
		}

		case ATTR_MISSILEEFFECT: {
			uint8_t missileEffect;
			if (!propStream.read<uint8_t>(missileEffect)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_MISSILEEFFECT, missileEffect);
			break;
		}

		case ATTR_KEYNUMBER: {
			uint16_t keyNumber;
			if (!propStream.read<uint16_t>(keyNumber)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_KEYNUMBER, keyNumber);
			break;
		}

		case ATTR_KEYHOLENUMBER:
		{
			uint16_t keyHoleNumber;
			if (!propStream.read<uint16_t>(keyHoleNumber)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_KEYHOLENUMBER, keyHoleNumber);
			break;
		}

		case ATTR_DOORLEVEL:
		{
			uint16_t doorLevel;
			if (!propStream.read<uint16_t>(doorLevel)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_DOORLEVEL, doorLevel);
			break;
		}

		case ATTR_DOORQUESTNUMBER:
		{
			uint16_t doorQuestNumber;
			if (!propStream.read<uint16_t>(doorQuestNumber)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_DOORQUESTNUMBER, doorQuestNumber);
			break;
		}

		case ATTR_DOORQUESTVALUE:
		{
			uint16_t doorQuestValue;
			if (!propStream.read<uint16_t>(doorQuestValue)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_DOORQUESTVALUE, doorQuestValue);
			break;
		}

		case ATTR_CHESTQUESTNUMBER:
		{
			uint16_t chestQuestNumber;
			if (!propStream.read<uint16_t>(chestQuestNumber)) {
				return ATTR_READ_ERROR;
			}

			setIntAttr(ITEM_ATTRIBUTE_CHESTQUESTNUMBER, chestQuestNumber);
			break;
		}

		case ATTR_LIFELEECH: {
			uint16_t lifeLeech;
			if (!propStream.read<uint16_t>(lifeLeech)) {
				return ATTR_READ_ERROR;
			}

			setLifeLeech(lifeLeech);
			break;
		}

		case ATTR_MANALEECH: {
			uint16_t manaLeech;
			if (!propStream.read<uint16_t>(manaLeech)) {
				return ATTR_READ_ERROR;
			}

			setManaLeech(manaLeech);
			break;
		}

		case ATTR_CRITICAL: {
			uint16_t critical;
			if (!propStream.read<uint16_t>(critical)) {
				return ATTR_READ_ERROR;
			}

			setCritical(critical);
			break;
		}

		case ATTR_REFLECTION: {
			uint16_t reflect;
			if (!propStream.read<uint16_t>(reflect)) {
				return ATTR_READ_ERROR;
			}

			setReflection(reflect);
			break;
		}

		//these should be handled through derived classes
		//If these are called then something has changed in the items.xml since the map was saved
		//just read the values

		//Depot class
		case ATTR_DEPOT_ID: {
			if (!propStream.skip(2)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		//Door class
		case ATTR_HOUSEDOORID: {
			if (!propStream.skip(1)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		//Bed class
		case ATTR_SLEEPERGUID: {
			if (!propStream.skip(4)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		case ATTR_SLEEPSTART: {
			if (!propStream.skip(4)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		//Teleport class
		case ATTR_TELE_DEST: {
			if (!propStream.skip(5)) {
				return ATTR_READ_ERROR;
			}
			break;
		}

		//Container class
		case ATTR_CONTAINER_ITEMS: {
			return ATTR_READ_ERROR;
		}

		default:
			return ATTR_READ_ERROR;
	}

	return ATTR_READ_CONTINUE;
}

bool Item::unserializeAttr(PropStream& propStream)
{
	uint8_t attr_type;
	while (propStream.read<uint8_t>(attr_type) && attr_type != 0) {
		Attr_ReadValue ret = readAttr(static_cast<AttrTypes_t>(attr_type), propStream);
		if (ret == ATTR_READ_ERROR) {
			return false;
		} else if (ret == ATTR_READ_END) {
			return true;
		}
	}
	return true;
}

bool Item::unserializeItemNode(FileLoader&, NODE, PropStream& propStream)
{
	return unserializeAttr(propStream);
}

void Item::serializeAttr(PropWriteStream& propWriteStream) const
{
	const ItemType& it = items[id];
	if (it.stackable || it.isFluidContainer() || it.isSplash()) {
		propWriteStream.write<uint8_t>(ATTR_COUNT);
		propWriteStream.write<uint8_t>(getSubType());
	}

	uint16_t charges = getCharges();
	if (charges != 0) {
		propWriteStream.write<uint8_t>(ATTR_CHARGES);
		propWriteStream.write<uint16_t>(charges);
	}

	if (it.moveable) {
		uint16_t actionId = getActionId();
		if (actionId != 0) {
			propWriteStream.write<uint8_t>(ATTR_ACTION_ID);
			propWriteStream.write<uint16_t>(actionId);
		}
	}

	const std::string& text = getText();
	if (!text.empty()) {
		propWriteStream.write<uint8_t>(ATTR_TEXT);
		propWriteStream.writeString(text);
	}

	const time_t writtenDate = getDate();
	if (writtenDate != 0) {
		propWriteStream.write<uint8_t>(ATTR_WRITTENDATE);
		propWriteStream.write<uint32_t>(writtenDate);
	}

	const std::string& writer = getWriter();
	if (!writer.empty()) {
		propWriteStream.write<uint8_t>(ATTR_WRITTENBY);
		propWriteStream.writeString(writer);
	}

	const std::string& specialDesc = getSpecialDescription();
	if (!specialDesc.empty()) {
		propWriteStream.write<uint8_t>(ATTR_DESC);
		propWriteStream.writeString(specialDesc);
	}

	if (hasAttribute(ITEM_ATTRIBUTE_DURATION)) {
		propWriteStream.write<uint8_t>(ATTR_DURATION);
		propWriteStream.write<uint32_t>(getIntAttr(ITEM_ATTRIBUTE_DURATION));
	}

	ItemDecayState_t decayState = getDecaying();
	if (decayState == DECAYING_TRUE || decayState == DECAYING_PENDING) {
		propWriteStream.write<uint8_t>(ATTR_DECAYING_STATE);
		propWriteStream.write<uint8_t>(decayState);
	}

	if (hasAttribute(ITEM_ATTRIBUTE_NAME)) {
		propWriteStream.write<uint8_t>(ATTR_NAME);
		propWriteStream.writeString(getStrAttr(ITEM_ATTRIBUTE_NAME));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_ARTICLE)) {
		propWriteStream.write<uint8_t>(ATTR_ARTICLE);
		propWriteStream.writeString(getStrAttr(ITEM_ATTRIBUTE_ARTICLE));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_PLURALNAME)) {
		propWriteStream.write<uint8_t>(ATTR_PLURALNAME);
		propWriteStream.writeString(getStrAttr(ITEM_ATTRIBUTE_PLURALNAME));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_WEIGHT)) {
		propWriteStream.write<uint8_t>(ATTR_WEIGHT);
		propWriteStream.write<uint32_t>(getIntAttr(ITEM_ATTRIBUTE_WEIGHT));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_ATTACK)) {
		propWriteStream.write<uint8_t>(ATTR_ATTACK);
		propWriteStream.write<int32_t>(getIntAttr(ITEM_ATTRIBUTE_ATTACK));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_DEFENSE)) {
		propWriteStream.write<uint8_t>(ATTR_DEFENSE);
		propWriteStream.write<int32_t>(getIntAttr(ITEM_ATTRIBUTE_DEFENSE));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_ARMOR)) {
		propWriteStream.write<uint8_t>(ATTR_ARMOR);
		propWriteStream.write<int32_t>(getIntAttr(ITEM_ATTRIBUTE_ARMOR));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_MISSILEEFFECT)) {
		propWriteStream.write<uint8_t>(ATTR_MISSILEEFFECT);
		propWriteStream.write<uint8_t>(getIntAttr(ITEM_ATTRIBUTE_MISSILEEFFECT));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_KEYNUMBER)) {
		propWriteStream.write<uint8_t>(ATTR_KEYNUMBER);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_KEYNUMBER));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_KEYHOLENUMBER)) {
		propWriteStream.write<uint8_t>(ATTR_KEYHOLENUMBER);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_KEYHOLENUMBER));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_DOORLEVEL)) {
		propWriteStream.write<uint8_t>(ATTR_DOORLEVEL);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_DOORLEVEL));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_DOORQUESTNUMBER)) {
		propWriteStream.write<uint8_t>(ATTR_DOORQUESTNUMBER);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_DOORQUESTNUMBER));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_DOORQUESTVALUE)) {
		propWriteStream.write<uint8_t>(ATTR_DOORQUESTVALUE);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_DOORQUESTVALUE));
	}

	if (hasAttribute(ITEM_ATTRIBUTE_CHESTQUESTNUMBER)) {
		propWriteStream.write<uint8_t>(ATTR_CHESTQUESTNUMBER);
		propWriteStream.write<uint16_t>(getIntAttr(ITEM_ATTRIBUTE_CHESTQUESTNUMBER));
	}

	uint16_t lifeLeechV = getLifeLeech();
	if (lifeLeechV != 0) {
		propWriteStream.write<uint8_t>(ATTR_LIFELEECH);
		propWriteStream.write<uint16_t>(lifeLeechV);
	}

	uint16_t manaLeechV = getManaLeech();
	if (manaLeechV != 0) {
		propWriteStream.write<uint8_t>(ATTR_MANALEECH);
		propWriteStream.write<uint16_t>(manaLeechV);
	}

	uint16_t criticalDmg = getCritical();
	if (criticalDmg != 0) {
		propWriteStream.write<uint8_t>(ATTR_CRITICAL);
		propWriteStream.write<uint16_t>(criticalDmg);
	}

	uint16_t reflectionV = getReflection();
	if (reflectionV != 0) {
		propWriteStream.write<uint8_t>(ATTR_REFLECTION);
		propWriteStream.write<uint16_t>(reflectionV);
	}
}

bool Item::hasProperty(ITEMPROPERTY prop) const
{
	const ItemType& it = items[id];
	switch (prop) {
		case CONST_PROP_BLOCKSOLID: return it.blockSolid;
		case CONST_PROP_MOVEABLE: return it.moveable;
		case CONST_PROP_HASHEIGHT: return it.hasHeight;
		case CONST_PROP_BLOCKPROJECTILE: return it.blockProjectile;
		case CONST_PROP_BLOCKPATH: return it.blockPathFind;
		case CONST_PROP_ISVERTICAL: return it.isVertical;
		case CONST_PROP_ISHORIZONTAL: return it.isHorizontal;
		case CONST_PROP_IMMOVABLEBLOCKSOLID: return it.blockSolid && !it.moveable;
		case CONST_PROP_IMMOVABLEBLOCKPATH: return it.blockPathFind && !it.moveable;
		case CONST_PROP_IMMOVABLENOFIELDBLOCKPATH: return !it.isMagicField() && it.blockPathFind && !it.moveable;
		case CONST_PROP_NOFIELDBLOCKPATH: return !it.isMagicField() && it.blockPathFind;
		case CONST_PROP_SUPPORTHANGABLE: return it.isHorizontal || it.isVertical;
		case CONST_PROP_UNLAY: return !it.allowPickupable;
		default: return false;
	}
}

uint32_t Item::getWeight() const
{
	uint32_t weight = getBaseWeight();
	if (isStackable()) {
		return weight * std::max<uint32_t>(1, getItemCount());
	}
	return weight;
}

std::string Item::getDescription(const ItemType& it, int32_t lookDistance,
	const Item* item /*= nullptr*/, int32_t subType /*= -1*/, bool addArticle /*= true*/)
{
	std::ostringstream s;
	s << getNameDescription(it, item, subType, addArticle);

	if (item) {
		subType = item->getSubType();
	}

	if (it.isRune()) {
		uint32_t charges = std::max(static_cast<uint32_t>(1), static_cast<uint32_t>(item == nullptr ? it.charges : item->getCharges()));

		if (it.runeLevel > 0) {
			s << " for level " << it.runeLevel;
		}

		if (it.runeLevel > 0) {
			s << " and";
		}

		s << " for magic level " << it.runeMagLevel;
		s << ". It's an \"" << it.runeSpellName << "\"-spell (" << charges << "x). ";
	} else if (it.isDoor() && item) {
		if (item->hasAttribute(ITEM_ATTRIBUTE_DOORLEVEL)) {
			s << " for level " << item->getIntAttr(ITEM_ATTRIBUTE_DOORLEVEL);
		}
		s << ".";
	} else if (it.weaponType != WEAPON_NONE) {
		bool begin = true;

		if (it.weaponType == WEAPON_DISTANCE && it.ammoType != AMMO_NONE) {
			if (it.attack != 0) {
				s << ", Atk" << std::showpos << it.attack << std::noshowpos;

				if(item && item->hasAttribute(ITEM_ATTRIBUTE_ATTACK)){
					s << "+" << static_cast<int>((item->getAttack() - it.attack));
				}
			}
		} else if (it.weaponType != WEAPON_AMMO && it.weaponType != WEAPON_WAND && (it.attack != 0 || it.defense != 0)) {
			s << " (";
			if (it.attack != 0) {
				s << "Atk:" << static_cast<int>(it.attack);
				if(item && item->hasAttribute(ITEM_ATTRIBUTE_ATTACK)){
					s << "+" << static_cast<int>((item->getAttack() - it.attack));
				}
			}

			if (it.defense != 0) {
				if (it.attack != 0)
					s << " ";
				s << "Def:" << static_cast<int>(it.defense);
				if(item && item->hasAttribute(ITEM_ATTRIBUTE_DEFENSE)){
					s << "+" << static_cast<int>((item->getDefense() - it.defense));
				}
			}

			uint16_t reflect = item->getReflection();
			if(reflect != 0){
				s << ", ";
				s << " reflect " << reflect << "%";
			}

			uint16_t lifeLeech = item->getLifeLeech();
			if(lifeLeech != 0){
				s << ", ";
				s << " hitpoints leech amount " << lifeLeech << "%";
			}

			uint16_t manaLeech = item->getManaLeech();
			if(manaLeech != 0){
				s << ", ";
				s << " mana points leech amount " << manaLeech << "%";
			}

			uint16_t critical = item->getCritical();
			if(critical != 0){
				s << ", ";
				s << " critical extra damage " << critical << "%";
			}

			if(it.abilities){
				if (it.abilities->stats[STAT_MAGICPOINTS]) {
					s << ", magic level " << std::showpos << it.abilities->stats[STAT_MAGICPOINTS] << std::noshowpos;
				}
				for (uint8_t i = SKILL_FIRST; i <= SKILL_LAST; i++) {
					if (!it.abilities->skills[i]) {
						continue;
					}

					s << ", ";

					s << getSkillName(i) << ' ' << std::showpos << it.abilities->skills[i] << std::noshowpos;
				}
			}
			s << ")";
		}
		
		s << ".";
	} else if (it.armor != 0) {
		bool begin = true;

		if (it.charges > 0) {
			if (subType > 1) {
				s << " that has " << static_cast<int32_t>(subType) << " charges left";
			} else {
				s << " that has " << it.charges << " charge left";
			}
		}

		s << " (Arm:" << it.armor;
		begin = false;
	
		uint16_t reflect = item->getReflection();
		if(reflect != 0){
			s << ", ";
			s << " reflect " << reflect << "%";
		}

		if(it.abilities){
			if(it.abilities->speed != 0){ 
				s << ", speed +" << it.abilities->speed;
			}

			if (it.abilities->stats[STAT_MAGICPOINTS]) {
				s << ", magic level " << std::showpos << it.abilities->stats[STAT_MAGICPOINTS] << std::noshowpos;
			}

			int16_t show = it.abilities->absorbPercent[0];
			if (show != 0) {
				for (size_t i = 1; i < COMBAT_COUNT; ++i) {
					if (it.abilities->absorbPercent[i] != show) {
						show = 0;
						break;
					}
				}
			}

			if (!show) {
				bool protectionBegin = true;
				for (size_t i = 0; i < COMBAT_COUNT; ++i) {
					if (it.abilities->absorbPercent[i] == 0) {
						continue;
					}

					if (protectionBegin) {
						protectionBegin = false;

						if (begin) {
							begin = false;
							s << " (";
						} else {
							s << ", ";
						}

						s << "protection ";
					} else {
						s << ", ";
					}

					s << getCombatName(indexToCombatType(i)) << ' ' << std::showpos << it.abilities->absorbPercent[i] << std::noshowpos << '%';
				}
			} else {
				if (begin) {
					begin = false;
					s << " (";
				} else {
					s << ", ";
				}

				s << "protection all " << std::showpos << show << std::noshowpos << '%';
			}
		}
		s << ").";
	} else if(it.abilities){ 
		if(it.abilities->speed != 0){
			s << "(Speed: +" << it.abilities->speed << ").";
		}
	} else if (it.isFluidContainer()) {
		if (item && item->getFluidType() != 0) {
			s << " of " << items[item->getFluidType()].name << ".";
		} else {
			s << ". It is empty.";
		}
	} else if (it.isSplash()) {
		s << " of ";
		if (item && item->getFluidType() != 0) {
			s << items[item->getFluidType()].name;
		} else {
			s << items[1].name;
		}
		s << ".";
	} else if (it.isContainer() && !it.isChest()) {
		s << " (Vol:" << static_cast<int>(it.maxItems) << ").";
	} else if (it.isKey()) {
		if (item) {
			s << " (Key:" << static_cast<int>(item->getIntAttr(ITEM_ATTRIBUTE_KEYNUMBER)) << ").";
		} else {
			s << " (Key:0).";
		}
	} else if (it.allowDistRead) {
		s << ".";
		s << std::endl;

		if (item && item->getText() != "") {
			if (lookDistance <= 4) {
				const std::string& writer = item->getWriter();
				if (!writer.empty()) {
					s << writer << " wrote";
					time_t date = item->getDate();
					if (date != 0) {
						s << " on " << formatDateShort(date);
					}
					s << ": ";
				} else {
					s << "You read: ";
				}
				s << item->getText();
			} else {
				s << "You are too far away to read it.";
			}
		} else {
			s << "Nothing is written on it.";
		}
	} else if (it.charges > 0) {
		uint32_t charges = (item == nullptr ? it.charges : item->getCharges());
		if (charges > 1) {
			s << " that has " << static_cast<int>(charges) << " charges left.";
		} else {
			s << " that has 1 charge left.";
		}
	} else if (it.showDuration) {
		if (item && item->hasAttribute(ITEM_ATTRIBUTE_DURATION)) {
			int32_t duration = item->getDuration() / 1000;
			s << " that has energy for ";

			if (duration >= 120) {
				s << duration / 60 << " minutes left.";
			} else if (duration > 60) {
				s << "1 minute left.";
			} else {
				s << "less than a minute left.";
			}
		} else {
			s << " that is brand-new.";
		}
	} else {
		s << ".";
	}

	if (it.wieldInfo != 0) {
		s << std::endl << "It can only be wielded properly by ";

		if (it.wieldInfo & WIELDINFO_PREMIUM) {
			s << "premium ";
		}

		if (it.wieldInfo & WIELDINFO_VOCREQ) {
			s << it.vocationString;
		} else {
			s << "players";
		}

		if (it.wieldInfo & WIELDINFO_LEVEL) {
			s << " of level " << static_cast<int>(it.minReqLevel) << " or higher";
		}

		if (it.wieldInfo & WIELDINFO_MAGLV) {
			if (it.wieldInfo & WIELDINFO_LEVEL) {
				s << " and";
			} else {
				s << " of";
			}

			s << " magic level " << static_cast<int>(it.minReqMagicLevel) << " or higher";
		}

		s << ".";
	}

	if (lookDistance <= 1 && !it.isChest() && it.pickupable) {
		double weight = (item == nullptr ? it.weight : item->getWeight());
		if (weight > 0) {
			s << std::endl << getWeightDescription(it, weight);
		}
	}

	if (item && item->getSpecialDescription() != "") {
		s << std::endl << item->getSpecialDescription().c_str();
	} else if (it.description.length() && lookDistance <= 1) {
		s << std::endl << it.description << ".";
	}

	return s.str();
}

std::string Item::getDescription(int32_t lookDistance) const
{
	const ItemType& it = items[id];
	return getDescription(it, lookDistance, this);
}

std::string Item::getNameDescription(const ItemType& it, const Item* item /*= nullptr*/, int32_t subType /*= -1*/, bool addArticle /*= true*/)
{
	if (item) {
		subType = item->getSubType();
	}

	std::ostringstream s;

	const std::string& name = (item ? item->getName() : it.name);
	if (!name.empty()) {
		if (it.stackable && subType > 1) {
			if (it.showCount) {
				s << subType << ' ';
			}

			s << (item ? item->getPluralName() : it.getPluralName());
		} else {
			if (addArticle) {
				const std::string& article = (item ? item->getArticle() : it.article);
				if (!article.empty()) {
					s << article << ' ';
				}
			}

			s << name;
		}
	} else {
		s << "an item of type " << it.id;
	}
	return s.str();
}

std::string Item::getNameDescription() const
{
	const ItemType& it = items[id];
	return getNameDescription(it, this);
}

std::string Item::getWeightDescription(const ItemType& it, uint32_t weight, uint32_t count /*= 1*/)
{
	std::ostringstream ss;
	if (it.stackable && count > 1 && it.showCount != 0) {
		ss << "They weigh ";
	} else {
		ss << "It weighs ";
	}

	if (weight < 10) {
		ss << "0.0" << weight;
	} else if (weight < 100) {
		ss << "0." << weight;
	} else {
		std::string weightString = std::to_string(weight);
		weightString.insert(weightString.end() - 2, '.');
		ss << weightString;
	}

	ss << " oz.";
	return ss.str();
}

std::string Item::getWeightDescription(uint32_t weight) const
{
	const ItemType& it = Item::items[id];
	return getWeightDescription(it, weight, getItemCount());
}

std::string Item::getWeightDescription() const
{
	uint32_t weight = getWeight();
	if (weight == 0) {
		return std::string();
	}
	return getWeightDescription(weight);
}

bool Item::canDecay() const
{
	if (isRemoved()) {
		return false;
	}

	const ItemType& it = Item::items[id];
	if (it.decayTo < 0 || it.decayTime == 0) {
		return false;
	}

	return true;
}

uint32_t Item::getWorth() const
{
	switch (id) {
		case ITEM_GOLD_COIN:
			return count;

		case ITEM_PLATINUM_COIN:
			return count * 100;

		case ITEM_CRYSTAL_COIN:
			return count * 10000;

		default:
			return 0;
	}
}

void Item::getLight(LightInfo& lightInfo) const
{
	const ItemType& it = items[id];
	lightInfo.color = it.lightColor;
	lightInfo.level = it.lightLevel;
}

std::string ItemAttributes::emptyString;

const std::string& ItemAttributes::getStrAttr(itemAttrTypes type) const
{
	if (!isStrAttrType(type)) {
		return emptyString;
	}

	const Attribute* attr = getExistingAttr(type);
	if (!attr) {
		return emptyString;
	}
	return *attr->value.string;
}

void ItemAttributes::setStrAttr(itemAttrTypes type, const std::string& value)
{
	if (!isStrAttrType(type)) {
		return;
	}

	if (value.empty()) {
		return;
	}

	Attribute& attr = getAttr(type);
	delete attr.value.string;
	attr.value.string = new std::string(value);
}

void ItemAttributes::removeAttribute(itemAttrTypes type)
{
	if (!hasAttribute(type)) {
		return;
	}

	auto prev_it = attributes.cbegin();
	if ((*prev_it).type == type) {
		attributes.pop_front();
	} else {
		auto it = prev_it, end = attributes.cend();
		while (++it != end) {
			if ((*it).type == type) {
				attributes.erase_after(prev_it);
				break;
			}
			prev_it = it;
		}
	}
	attributeBits &= ~type;
}

int64_t ItemAttributes::getIntAttr(itemAttrTypes type) const
{
	if (!isIntAttrType(type)) {
		return 0;
	}

	const Attribute* attr = getExistingAttr(type);
	if (!attr) {
		return 0;
	}
	return attr->value.integer;
}

void ItemAttributes::setIntAttr(itemAttrTypes type, int64_t value)
{
	if (!isIntAttrType(type)) {
		return;
	}

	getAttr(type).value.integer = value;
}

void ItemAttributes::increaseIntAttr(itemAttrTypes type, int64_t value)
{
	if (!isIntAttrType(type)) {
		return;
	}

	getAttr(type).value.integer += value;
}

const ItemAttributes::Attribute* ItemAttributes::getExistingAttr(itemAttrTypes type) const
{
	if (hasAttribute(type)) {
		for (const Attribute& attribute : attributes) {
			if (attribute.type == type) {
				return &attribute;
			}
		}
	}
	return nullptr;
}

ItemAttributes::Attribute& ItemAttributes::getAttr(itemAttrTypes type)
{
	if (hasAttribute(type)) {
		for (Attribute& attribute : attributes) {
			if (attribute.type == type) {
				return attribute;
			}
		}
	}

	attributeBits |= type;
	attributes.emplace_front(type);
	return attributes.front();
}

void Item::startDecaying()
{
	g_game.startDecay(this);
}