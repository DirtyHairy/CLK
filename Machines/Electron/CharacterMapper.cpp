//
//  CharacterMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CharacterMapper.hpp"
#include "Electron.hpp"

using namespace Electron;

uint16_t *CharacterMapper::sequence_for_character(char character) {
#define KEYS(...)	{__VA_ARGS__, EndSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, EndSequence}
#define CTRL(...)	{KeyControl, __VA_ARGS__, EndSequence}
#define X			{NotMapped}
	static KeySequence key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	KEYS(KeyDelete),			/* HT */	X,
		/* LF */	KEYS(KeyReturn),			/* VT */	X,
		/* FF */	X,							/* CR */	X,
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		SHIFT(Key1),
		/* " */		SHIFT(Key2),				/* # */		SHIFT(Key3),
		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
		/* & */		SHIFT(Key6),				/* ' */		SHIFT(Key7),
		/* ( */		SHIFT(Key8),				/* ) */		SHIFT(Key9),
		/* * */		SHIFT(KeyColon),			/* + */		SHIFT(KeySemiColon),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemiColon),
		/* < */		SHIFT(KeyComma),			/* = */		SHIFT(KeyMinus),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
		/* @ */		SHIFT(Key0),				/* A */		KEYS(KeyA),
		/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
		/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
		/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
		/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
		/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
		/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
		/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
		/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
		/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
		/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
		/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
		/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyCopy),
		/* \ */		CTRL(KeyRight),				/* ] */		CTRL(KeyCopy),
		/* ^ */		SHIFT(KeyLeft),				/* _ */		SHIFT(KeyDown),
		/* ` */		X,							/* a */		SHIFT(KeyA),
		/* b */		SHIFT(KeyB),				/* c */		SHIFT(KeyC),
		/* d */		SHIFT(KeyD),				/* e */		SHIFT(KeyE),
		/* f */		SHIFT(KeyF),				/* g */		SHIFT(KeyG),
		/* h */		SHIFT(KeyH),				/* i */		SHIFT(KeyI),
		/* j */		SHIFT(KeyJ),				/* k */		SHIFT(KeyK),
		/* l */		SHIFT(KeyL),				/* m */		SHIFT(KeyM),
		/* n */		SHIFT(KeyN),				/* o */		SHIFT(KeyO),
		/* p */		SHIFT(KeyP),				/* q */		SHIFT(KeyQ),
		/* r */		SHIFT(KeyR),				/* s */		SHIFT(KeyS),
		/* t */		SHIFT(KeyT),				/* u */		SHIFT(KeyU),
		/* v */		SHIFT(KeyV),				/* w */		SHIFT(KeyW),
		/* x */		SHIFT(KeyX),				/* y */		SHIFT(KeyY),
		/* z */		SHIFT(KeyZ),				/* { */		CTRL(KeyUp),
		/* | */		SHIFT(KeyRight),			/* } */		CTRL(KeyDown),
		/* ~ */		CTRL(KeyLeft)
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, sizeof(key_sequences), character);
}
