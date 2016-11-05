//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

uint16_t *Commodore::Vic20::Machine::sequence_for_character(Utility::Typer *typer, char character)
{
#define KEYS(...)	{__VA_ARGS__, TerminateSequence}
#define SHIFT(...)	{KeyLShift, __VA_ARGS__, TerminateSequence}
#define X			{NotMapped}
	static Key key_sequences[][3] = {
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
		/* * */		KEYS(KeyAsterisk),			/* + */		KEYS(KeyPlus),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyDash),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemicolon),
		/* < */		SHIFT(KeyComma),			/* = */		KEYS(KeyEquals),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
		/* @ */		KEYS(KeyAt),				/* A */		KEYS(KeyA),
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
		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyColon),
		/* \ */		X,							/* ] */		SHIFT(KeySemicolon),
		/* ^ */		X,							/* _ */		X,
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
		/* z */		KEYS(KeyZ)
	};
#undef KEYS
#undef SHIFT
#undef X

	if(character > sizeof(key_sequences) / sizeof(*key_sequences)) return nullptr;
	if(key_sequences[character][0] == NotMapped) return nullptr;
	return (uint16_t *)key_sequences[character];
}
