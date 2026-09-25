// Проверка ChatGuard на живом сервере, запускается через server_test.bat.
// Файл хранится в UTF-8; server_test.ps1 перекодирует его в cp1251, как у настоящего мода.

#include <open.mp>
#include <chatguard>

new g_checks;
new g_failures;

Check(bool:ok, const what[])
{
	g_checks++;
	if (!ok)
	{
		g_failures++;
		printf("[chatguard-test] FAIL: %s", what);
	}
}

main()
{
}

public OnGameModeInit()
{
	Check(ChatGuard_CheckText("всем привет, как дела?") == CHATGUARD_OK, "clean text (config defaults applied)");
	Check(ChatGuard_CheckText("заходите 185.169.134.67:7777") == CHATGUARD_ADVERTISING, "IP address");
	Check(ChatGuard_CheckText("лучший сервер samp-rp . ru") == CHATGUARD_ADVERTISING, "spaced-out domain");
	Check(ChatGuard_CheckText("наш форум mysite.ru") == CHATGUARD_OK, "allowed_hosts from config.json");
	Check(ChatGuard_CheckText("ВСЕМ ПРИВЕТ") == CHATGUARD_CAPS, "caps");
	Check(ChatGuard_CheckText("ты ДуРаК") == CHATGUARD_BAD_WORD, "bad_words from config.json");
	Check(ChatGuard_CheckText("ну ты и сверхтупица") == CHATGUARD_BAD_WORD, "bad_words wildcard from config.json");

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
	Check(!ChatGuard_IsImmune(bot), "not immune by default");
	Check(ChatGuard_SetImmune(bot, true), "SetImmune succeeds");
	Check(ChatGuard_IsImmune(bot), "immune after SetImmune");
	ChatGuard_SetImmune(bot, false);
	Check(!ChatGuard_IsImmune(bot), "not immune after revoke");
	Check(!ChatGuard_SetImmune(999, true), "SetImmune fails for a missing player");

	printf("[chatguard-test] done: %d checks, %d failed", g_checks, g_failures);
	SendRconCommand("exit");
}

public OnChatGuardBlock(playerid, CHATGUARD_REASON:reason, const text[])
{
	printf("[chatguard-test] blocked player %d, reason %d: %s", playerid, _:reason, text);
	return 1;
}
