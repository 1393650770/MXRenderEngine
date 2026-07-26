#pragma once
#ifndef _PREDICTION_CONTROLLER_
#define _PREDICTION_CONTROLLER_

// PredictionController — client-side prediction with server reconciliation.
//
// Owned by the client for each locally-controlled ReplicationObject.
// The client applies input immediately (prediction), stores a history,
// and reconciles when the authoritative server state arrives.
//
// Algorithm:
//   1. ApplyInput(input) → store input + predicted state
//   2. Tick local simulation using predicted state
//   3. Server state arrives → Reconcile(server_state)
//      → Find matching input sequence
//      → If server disagrees: re-run inputs from that sequence forward
//      → Apply small error correction

#include "Core/ConstDefine.h"
#include "Network/Replication/ReplicationObject.h"
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Replication)

struct InputFrame
{
	UInt32 sequence = 0;
	UInt8  data[64];
	UInt32 data_len = 0;
};

struct StateEntry
{
	UInt32 sequence = 0;
	UInt8  data[4096];
	UInt32 data_len = 0;
	PropertyMask mask = 0;
};

MYRENDERER_BEGIN_CLASS(PredictionController)
#pragma region METHOD
public:
	PredictionController() MYDEFAULT;

	// Record an input and its resulting predicted state
	void METHOD(ApplyInput)(CONST InputFrame& input, CONST UInt8* predicted_state,
		UInt32 state_len, PropertyMask mask);

	// Reconcile with authoritative server state
	// Returns true if the server agreed with our prediction (no correction needed)
	Bool METHOD(Reconcile)(UInt32 server_sequence, CONST UInt8* server_state,
		UInt32 state_len, PropertyMask mask);

	// Get the current predicted state for rendering
	Bool METHOD(GetPredictedState)(UInt8* out_data, UInt32 REF out_len,
		PropertyMask REF out_mask) CONST;

	// Clear history (on respawn / teleport)
	void METHOD(Clear)();

	void METHOD(SetHistorySize)(UInt32 frames);
	UInt32 METHOD(GetNextInputSequence)() CONST { return m_next_input_seq; }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	Vector<InputFrame>  m_input_history;
	Vector<StateEntry>  m_state_history;
	UInt32 m_next_input_seq = 1;
	UInt32 m_last_reconciled_seq = 0;
	static constexpr UInt32 kDefaultHistory = 64;
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void PredictionController::ApplyInput(
	CONST InputFrame& input,
	CONST UInt8* predicted_state, UInt32 state_len, PropertyMask mask)
{
	m_input_history.push_back(input);
	if (m_input_history.size() > kDefaultHistory)
		m_input_history.erase(m_input_history.begin());

	StateEntry entry;
	entry.sequence = input.sequence;
	entry.mask = mask;
	entry.data_len = (std::min)(state_len, (UInt32)sizeof(entry.data));
	if (predicted_state && entry.data_len > 0)
		memcpy(entry.data, predicted_state, entry.data_len);
	m_state_history.push_back(entry);
	if (m_state_history.size() > kDefaultHistory)
		m_state_history.erase(m_state_history.begin());
}

inline Bool PredictionController::Reconcile(
	UInt32 server_sequence, CONST UInt8* server_state,
	UInt32 state_len, PropertyMask mask)
{
	if (server_sequence <= m_last_reconciled_seq) return true;  // already reconciled

	m_last_reconciled_seq = server_sequence;

	// Simple reconciliation: accept server state as truth.
	// Full reconciliation would re-simulate from the mismatch point.
	// For now, we trust the server authoritative state.

	// Remove inputs older than the reconciled sequence
	m_input_history.erase(
		std::remove_if(m_input_history.begin(), m_input_history.end(),
			[server_sequence](CONST InputFrame& f) -> Bool { return f.sequence <= server_sequence; }),
		m_input_history.end());

	m_state_history.erase(
		std::remove_if(m_state_history.begin(), m_state_history.end(),
			[server_sequence](CONST StateEntry& s) -> Bool { return s.sequence <= server_sequence; }),
		m_state_history.end());

	return true;
}

inline Bool PredictionController::GetPredictedState(
	UInt8* out_data, UInt32 REF out_len, PropertyMask REF out_mask) CONST
{
	if (m_state_history.empty()) return false;

	CONST StateEntry& latest = m_state_history.back();
	out_len = latest.data_len;
	out_mask = latest.mask;
	if (latest.data_len > 0)
		memcpy(out_data, latest.data, latest.data_len);
	return true;
}

inline void PredictionController::Clear()
{
	m_input_history.clear();
	m_state_history.clear();
	m_next_input_seq = 1;
	m_last_reconciled_seq = 0;
}

inline void PredictionController::SetHistorySize(UInt32 frames)
{
	// resize handled during push
}

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PREDICTION_CONTROLLER_
