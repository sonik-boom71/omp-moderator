// Проверка Модератора на живом сервере, запускается через server_test.bat.
// Файл хранится в UTF-8; server_test.ps1 перекодирует его в cp1251, как у настоящего мода.

#include <open.mp>
#include <moderator>

new g_checks;
new g_failures;

Check(bool:ok, const what[])
{
	g_checks++;
	if (!ok)
	{
		g_failures++;
		printf("[moderator-test] FAIL: %s", what);
	}
}

main()
{
}

public OnGameModeInit()
{
	Check(Moderator_CheckText("всем привет, как дела?") == MODERATOR_OK, "clean text (config defaults applied)");
	Check(Moderator_CheckText("заходите 185.169.134.67:7777") == MODERATOR_ADVERTISING, "IP address");
	Check(Moderator_CheckText("лучший сервер samp-rp . ru") == MODERATOR_ADVERTISING, "spaced-out domain");
	Check(Moderator_CheckText("наш форум mysite.ru") == MODERATOR_OK, "allowed_hosts from config.json");
	Check(Moderator_CheckText("ВСЕМ ПРИВЕТ") == MODERATOR_CAPS, "caps");
	Check(Moderator_CheckText("ты ДуРаК") == MODERATOR_BAD_WORD, "bad_words from config.json");
	Check(Moderator_CheckText("ну ты и сверхтупица") == MODERATOR_BAD_WORD, "bad_words wildcard from config.json");

	NPC_Create("Test_Bot");
	SetTimer("RunPlayerChecks", 500, false);
	return 1;
}

forward RunPlayerChecks();
public RunPlayerChecks()
{
	new bot = INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && IsPlayerNPC(i))
		{
			bot = i;
			break;
		}
	}
	Check(bot != INVALID_PLAYER_ID, "NPC is a connected player");
	Check(!Moderator_IsImmune(bot), "not immune by default");
	Check(Moderator_SetImmune(bot, true), "SetImmune succeeds");
	Check(Moderator_IsImmune(bot), "immune after SetImmune");
	Moderator_SetImmune(bot, false);
	Check(!Moderator_IsImmune(bot), "not immune after revoke");
	Check(!Moderator_SetImmune(999, true), "SetImmune fails for a missing player");

	printf("[moderator-test] done: %d checks, %d failed", g_checks, g_failures);
	SendRconCommand("exit");
}

public OnModeratorBlock(playerid, MODERATOR_REASON:reason, const text[])
{
	printf("[moderator-test] blocked player %d, reason %d: %s", playerid, _:reason, text);
	return 1;
}
