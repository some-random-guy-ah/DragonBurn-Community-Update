#pragma once
#include <fstream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "memory/imemory.h"

struct Offsets {
    struct {
        uint32_t dwEntityList, dwViewMatrix, dwViewRender, dwLocalPlayerPawn, dwLocalPlayerController, dwGlobalVars, dwPlantedC4, dwWeaponC4;
    } client;
    struct {
        uint32_t m_iTeamNum, m_pGameSceneNode, m_iHealth;
    } C_BaseEntity;
    struct {
        uint32_t m_vecAbsOrigin;
    } CGameSceneNode;
    struct {
        uint32_t m_modelState;
    } CSkeletonInstance;
    struct {
        uint32_t m_hPlayerPawn;
        uint32_t m_hPawn;
        uint32_t m_sSanitizedPlayerName;
        uint32_t m_hObserverPawn;
    } CCSPlayerController;
    struct {
        uint32_t m_pObserverServices;
    } C_BasePlayerPawn;
    struct {
        uint32_t m_hObserverTarget;
    } CPlayer_ObserverServices;
    struct {
        uint32_t m_bIsScoped, m_entitySpottedState;
    } C_CSPlayerPawn;
    struct {
        uint32_t m_vecViewOffset;
    } C_BaseModelEntity;
    struct {
        uint32_t m_pWeaponServices;
    } C_CSPlayerPawnBase;
    struct {
        uint32_t m_hActiveWeapon;
        uint32_t m_hMyWeapons;
    } CPlayer_WeaponServices;
    struct {
        uint32_t m_AttributeManager;
    } C_EconEntity;
    struct {
        uint32_t m_Item;
    } C_AttributeContainer;
    struct {
        uint32_t m_iItemDefinitionIndex;
    } C_EconItemView;
    struct {
        uint32_t m_bSpotted;
    } EntitySpottedState_t;

    struct {
        uint32_t m_bBeingDefused;
        uint32_t m_flDefuseCountDown;
        uint32_t m_nBombSite;
        uint32_t m_flC4Blow;
    } C4;

    bool load(const std::string& offsets_path, const std::string& client_dll_path) {
        if (!std::filesystem::exists(offsets_path) ||
            !std::filesystem::exists(client_dll_path)) {
            printf("[!] Offset files not found, running cs2-dumper...\n");
            if (!run_dumper()) return false;
        }

        if (!parse_offsets(offsets_path, client_dll_path)) {
            printf("[!] Failed to parse offsets. Running cs2-dumper...\n");
            if (!run_dumper()) return false;
            if (!parse_offsets(offsets_path, client_dll_path)) {
                printf("[!] Still failed after re-dump.\n");
                return false;
            }
        }

        auto result = functional_test();
        if (result == TestResult::OFFSETS_WRONG) {
            printf("[!] Offsets are outdated (can't read valid game data). Running cs2-dumper...\n");
            if (!run_dumper()) return false;
            if (!parse_offsets(offsets_path, client_dll_path)) return false;
            result = functional_test();
            if (result == TestResult::OFFSETS_WRONG) {
                printf("[!] Offsets still invalid after re-dump. Game may have updated.\n");
                return false;
            }
        }

        if (result == TestResult::NO_PLAYERS) {
            printf("[*] Offsets parsed OK but no players found (you may not be in a match)\n");
            printf("[*] Proceeding — offsets will be validated when players are present\n");
        } else {
            printf("[+] Offsets validated: successfully read player data\n");
        }

        return true;
    }

private:
    enum class TestResult { OK, NO_PLAYERS, OFFSETS_WRONG };

    bool parse_offsets(const std::string& offsets_path, const std::string& client_dll_path) {
        try {
            nlohmann::json oj, cj;
            {
                std::ifstream f(offsets_path);
                if (!f) return false;
                f >> oj;
            }
            {
                std::ifstream f(client_dll_path);
                if (!f) return false;
                f >> cj;
            }

            auto& cl = oj["client.dll"];
            client.dwEntityList = cl["dwEntityList"];
            client.dwViewMatrix = cl["dwViewMatrix"];
            client.dwViewRender = cl["dwViewRender"];
            client.dwLocalPlayerPawn = cl["dwLocalPlayerPawn"];
            client.dwLocalPlayerController = cl["dwLocalPlayerController"];
            client.dwGlobalVars = cl["dwGlobalVars"];
            client.dwPlantedC4 = cl.value("dwPlantedC4", 0u);
            client.dwWeaponC4 = cl.value("dwWeaponC4", 0u);

            auto& cs = cj["client.dll"]["classes"];
            C_BaseEntity.m_iTeamNum = cs["C_BaseEntity"]["fields"]["m_iTeamNum"];
            C_BaseEntity.m_pGameSceneNode = cs["C_BaseEntity"]["fields"]["m_pGameSceneNode"];
            C_BaseEntity.m_iHealth = cs["C_BaseEntity"]["fields"]["m_iHealth"];
            CGameSceneNode.m_vecAbsOrigin = cs["CGameSceneNode"]["fields"]["m_vecAbsOrigin"];
            CSkeletonInstance.m_modelState = cs["CSkeletonInstance"]["fields"]["m_modelState"];

            CCSPlayerController.m_hPlayerPawn =
                cs["CCSPlayerController"]["fields"]["m_hPlayerPawn"];
            CCSPlayerController.m_sSanitizedPlayerName =
                cs["CCSPlayerController"]["fields"]["m_sSanitizedPlayerName"];
            CCSPlayerController.m_hPawn =
                cs["CBasePlayerController"]["fields"]["m_hPawn"];

            C_BasePlayerPawn.m_pObserverServices =
                cs["C_BasePlayerPawn"]["fields"]["m_pObserverServices"];
            CPlayer_ObserverServices.m_hObserverTarget =
                cs["CPlayer_ObserverServices"]["fields"]["m_hObserverTarget"];

            C_CSPlayerPawn.m_bIsScoped = cs["C_CSPlayerPawn"]["fields"]["m_bIsScoped"];
            C_BaseModelEntity.m_vecViewOffset =
                cs["C_BaseModelEntity"]["fields"]["m_vecViewOffset"];

            // Weapon reading offsets
            C_CSPlayerPawnBase.m_pWeaponServices = cs["C_BasePlayerPawn"]["fields"]["m_pWeaponServices"];
            CPlayer_WeaponServices.m_hActiveWeapon = cs["CPlayer_WeaponServices"]["fields"]["m_hActiveWeapon"];
            CPlayer_WeaponServices.m_hMyWeapons = cs["CPlayer_WeaponServices"]["fields"]["m_hMyWeapons"];

            C_EconEntity.m_AttributeManager = cs["C_EconEntity"]["fields"]["m_AttributeManager"];
            C_AttributeContainer.m_Item = cs["C_AttributeContainer"]["fields"]["m_Item"];
            C_EconItemView.m_iItemDefinitionIndex = cs["C_EconItemView"]["fields"]["m_iItemDefinitionIndex"];

            C_CSPlayerPawn.m_entitySpottedState = cs["C_CSPlayerPawn"]["fields"]["m_entitySpottedState"];
            EntitySpottedState_t.m_bSpotted = cs["EntitySpottedState_t"]["fields"]["m_bSpotted"];

            if (cs.contains("C_PlantedC4") && cs["C_PlantedC4"].contains("fields")) {
                auto& c4f = cs["C_PlantedC4"]["fields"];
                C4.m_bBeingDefused = c4f.value("m_bBeingDefused", 0u);
                C4.m_flDefuseCountDown = c4f.value("m_flDefuseCountDown", 0u);
                C4.m_nBombSite = c4f.value("m_nBombSite", 0u);
                C4.m_flC4Blow = c4f.value("m_flC4Blow", 0u);
            }

            return true;
        } catch (const std::exception& e) {
            printf("[!] Offset parse error: %s\n", e.what());
            return false;
        }
    }

    TestResult functional_test() {
        uintptr_t client_base = g_memory->get_client_base();
        if (!client_base) {
            printf("[!] client_base is 0\n");
            return TestResult::OFFSETS_WRONG;
        }

        uintptr_t entity_list = g_memory->read<uintptr_t>(client_base + client.dwEntityList);
        if (!entity_list) {
            printf("[!] entity_list is null (offset 0x%X)\n", client.dwEntityList);
            return TestResult::OFFSETS_WRONG;
        }

        uintptr_t first_page = g_memory->read<uintptr_t>(entity_list + 16);
        if (!first_page) {
            return TestResult::NO_PLAYERS;
        }

        int valid_players = 0;
        int bogus_reads = 0;

        for (int i = 1; i < 64; i++) {
            uintptr_t controller = g_memory->read<uintptr_t>(first_page + 112 * (i & 0x1FF));
            if (!controller) continue;

            uint32_t pawn_handle = g_memory->read<uint32_t>(
                controller + CCSPlayerController.m_hPawn);
            if (!pawn_handle) {
                pawn_handle = g_memory->read<uint32_t>(
                    controller + CCSPlayerController.m_hPlayerPawn);
            }
            if (!pawn_handle) continue;

            uintptr_t pawn_page = g_memory->read<uintptr_t>(
                entity_list + 8 * ((pawn_handle & 0x7FFF) >> 9) + 16);
            if (!pawn_page) continue;

            uintptr_t pawn = g_memory->read<uintptr_t>(
                pawn_page + 112 * (pawn_handle & 0x1FF));
            if (!pawn) continue;

            int team = g_memory->read<int>(pawn + C_BaseEntity.m_iTeamNum);
            int health = g_memory->read<int>(pawn + C_BaseEntity.m_iHealth);

            if (team >= 0 && team <= 3) {
                if (health >= 0 && health <= 100) {
                    valid_players++;
                } else if (health > 100 || health < -1) {
                    bogus_reads++;
                }
            } else {
                bogus_reads++;
            }
        }

        if (valid_players > 0 && bogus_reads <= valid_players) {
            printf("[+] Found %d valid players in functional test\n", valid_players);
            return TestResult::OK;
        }

        if (valid_players == 0 && bogus_reads == 0) {
            return TestResult::NO_PLAYERS;
        }

        printf("[!] Functional test failed: %d valid, %d bogus reads\n",
               valid_players, bogus_reads);
        return TestResult::OFFSETS_WRONG;
    }

    bool run_dumper() {
        const char* dumper = "cs2-dumper.exe";

        if (!std::filesystem::exists(dumper)) {
            printf("[!] %s not found in current directory\n", dumper);
            printf("[!] Download from https://github.com/a2x/cs2-dumper/releases\n");
            printf("[!] Place next to this executable and restart.\n");
            return false;
        }

        printf("[*] Running %s...\n", dumper);
        int ret = system(dumper);
        if (ret != 0) {
            printf("[!] cs2-dumper failed with code %d\n", ret);
            return false;
        }

        const char* dump_dir = "output";
        if (!std::filesystem::exists(dump_dir)) {
            printf("[!] cs2-dumper output directory '%s' not found\n", dump_dir);
            return false;
        }

        std::filesystem::create_directories("offsets");

        struct FileCopy {
            const char* src;
            const char* dst;
        };
        static const FileCopy needed[] = {
            {"output/offsets.json",    "offsets/offsets.json"},
            {"output/client_dll.json", "offsets/client_dll.json"},
        };

        bool ok = true;
        for (const auto& fc : needed) {
            if (std::filesystem::exists(fc.src)) {
                try {
                    if (std::filesystem::exists(fc.dst))
                        std::filesystem::remove(fc.dst);
                    std::filesystem::copy_file(fc.src, fc.dst);
                    printf("[+] Copied %s -> %s\n", fc.src, fc.dst);
                } catch (const std::exception& e) {
                    printf("[!] Failed to copy %s: %s\n", fc.src, e.what());
                    ok = false;
                }
            } else {
                printf("[!] Expected file %s not found in dumper output\n", fc.src);
                ok = false;
            }
        }

        try {
            std::filesystem::remove_all(dump_dir);
            printf("[+] Cleaned up %s/\n", dump_dir);
        } catch (const std::exception& e) {
            printf("[!] Failed to clean up %s: %s\n", dump_dir, e.what());
        }

        return ok;
    }
};

inline Offsets g_offsets;