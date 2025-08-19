#pragma once

//==============================================================================
/**
 * Sdílené konstanty pro Ithaca synth.
 * - Slouží k centrální konfiguraci (např. polyfonie, pitch shift).
 */
struct Config
{
    static constexpr int MAX_VOICES = 16;  // Maximální počet hlasů pro polyfonii (zdůvodnění: Z DESIGN7.md, až 16 hlasů).
    static constexpr int MAX_PITCH_SHIFT = 12;  // Maximální pitch shift v půltónech (zdůvodnění: Z DESIGN7.md).
    // Další konstanty lze přidat (např. VELOCITY_LEVELS = 8 z Python skriptu).
};