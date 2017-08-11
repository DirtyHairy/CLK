//
//  CharacterMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CharacterMapper.hpp"
#include "AmstradCPC.hpp"

using namespace AmstradCPC;

uint16_t *CharacterMapper::sequence_for_character(char character) {
#define KEYS(...)	{__VA_ARGS__, EndSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, EndSequence}
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
		/* * */		SHIFT(KeyColon),			/* + */		SHIFT(KeySemicolon),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeyForwardSlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemicolon),
		/* < */		SHIFT(KeyComma),			/* = */		SHIFT(KeyMinus),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeyForwardSlash),
		/* @ */		SHIFT(KeyAt),				/* A */		SHIFT(KeyA),
		/* B */		SHIFT(KeyB),				/* C */		SHIFT(KeyC),
		/* D */		SHIFT(KeyD),				/* E */		SHIFT(KeyE),
		/* F */		SHIFT(KeyF),				/* G */		SHIFT(KeyG),
		/* H */		SHIFT(KeyH),				/* I */		SHIFT(KeyI),
		/* J */		SHIFT(KeyJ),				/* K */		SHIFT(KeyK),
		/* L */		SHIFT(KeyL),				/* M */		SHIFT(KeyM),
		/* N */		SHIFT(KeyN),				/* O */		SHIFT(KeyO),
		/* P */		SHIFT(KeyP),				/* Q */		SHIFT(KeyQ),
		/* R */		SHIFT(KeyR),				/* S */		SHIFT(KeyS),
		/* T */		SHIFT(KeyT),				/* U */		SHIFT(KeyU),
		/* V */		SHIFT(KeyV),				/* W */		SHIFT(KeyW),
		/* X */		SHIFT(KeyX),				/* Y */		SHIFT(KeyY),
		/* Z */		SHIFT(KeyZ),				/* [ */		KEYS(KeyLeftSquareBracket),
		/* \ */		KEYS(KeyBackSlash),			/* ] */		KEYS(KeyRightSquareBracket),
		/* ^ */		SHIFT(KeyCaret),			/* _ */		SHIFT(Key0),
		/* ` */		X,							/* a */		KEYS(KeyA),
		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
		/* z */		KEYS(KeyZ),					/* { */		X,
		/* | */		SHIFT(KeyAt),				/* } */		X,
		/* ~ */		X
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, sizeof(key_sequences), character);
}
