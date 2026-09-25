// ChatGuard — модерация чата для open.mp: антиреклама, антикапс, антифлуд, фильтр слов.
// Настройки — в разделе "chatguard" файла config.json.

#if defined _chatguard_included
	#endinput
#endif
#define _chatguard_included

enum CHATGUARD_REASON
{
	CHATGUARD_OK,
	CHATGUARD_FLOOD,
	CHATGUARD_REPEAT,
	CHATGUARD_ADVERTISING,
	CHATGUARD_CAPS,
	CHATGUARD_BAD_WORD
}

// Проверяет текст (например, в /pm или /ad) на рекламу, запрещённые слова и капс.
// Флуд и повторы считаются только для обычного чата.
native CHATGUARD_REASON:ChatGuard_CheckText(const text[]);

// Сообщения игрока с иммунитетом (например, админа) в чате не проверяются.
// Сбрасывается при переподключении игрока и перезапуске мода.
native bool:ChatGuard_SetImmune(playerid, bool:immune);
native bool:ChatGuard_IsImmune(playerid);

// Вызывается, когда сообщение в чате заблокировано.
// Верните 0, чтобы игрок не получил стандартное предупреждение (например, если выдаёте своё).
forward OnChatGuardBlock(playerid, CHATGUARD_REASON:reason, const text[]);
