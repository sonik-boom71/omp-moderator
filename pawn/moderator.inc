// Модератор — модерация чата для open.mp: антиреклама, антикапс, антифлуд, фильтр слов.
// Настройки — в разделе "moderator" файла config.json.

#if defined _moderator_included
	#endinput
#endif
#define _moderator_included

enum MODERATOR_REASON
{
	MODERATOR_OK,
	MODERATOR_FLOOD,
	MODERATOR_REPEAT,
	MODERATOR_ADVERTISING,
	MODERATOR_CAPS,
	MODERATOR_BAD_WORD
}

// Проверяет текст (например, в /pm или /ad) на рекламу, запрещённые слова и капс.
// Флуд и повторы считаются только для обычного чата.
native MODERATOR_REASON:Moderator_CheckText(const text[]);

// Сообщения игрока с иммунитетом (например, админа) в чате не проверяются.
// Сбрасывается при переподключении игрока и перезапуске мода.
native bool:Moderator_SetImmune(playerid, bool:immune);
native bool:Moderator_IsImmune(playerid);

// Вызывается, когда сообщение в чате заблокировано.
// Верните 0, чтобы игрок не получил стандартное предупреждение (например, если выдаёте своё).
forward OnModeratorBlock(playerid, MODERATOR_REASON:reason, const text[]);
