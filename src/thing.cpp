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

#include "thing.h"
#include "tile.h"

const Position& Thing::getPosition() const
{
	const Tile* tile = getTile();
	if (!tile) {
		return Tile::nullptr_tile.getPosition();
	}
	return tile->getPosition();
}

Tile* Thing::getTile()
{
	return dynamic_cast<Tile*>(this);
}

const Tile* Thing::getTile() const
{
	return dynamic_cast<const Tile*>(this);
}
