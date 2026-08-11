#pragma once
#ifndef _RMLUIHOTRELOADSERVICE_
#define _RMLUIHOTRELOADSERVICE_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

/// Result of one polling pass — a plain value type built on the LOGIC thread
/// and passed BY VALUE into a render-thread command (no shared state).
struct UIHotReloadChangeSet
{
	bool rcss_changed = false;          // any .rcss changed → reload stylesheets
	Vector<String> rml_changed_paths;   // changed .rml relative paths, e.g. "RmlUI/DemoPanel.rml"

	bool empty() const { return !rcss_changed && rml_changed_paths.empty(); }
};

/**
 * File-watcher for UI source files (RML/RCSS), RmlUi-agnostic (pure file
 * state machine — reusable by future non-RmlUI backends).
 *
 * Thread affinity (must be respected, see RmlUISystem):
 *   - ALL members are owned by the LOGIC thread: Enable/TrackPath/Poll are
 *     logic-thread-only.
 *   - Detection only; application of changes is the backend's job (it enqueues
 *     render-thread commands with the returned change set by value).
 *
 * Watches TWO files per path:
 *   - output  = cwd-relative (what RmlUi actually loads, e.g. build output dir)
 *   - source  = <project root>/resource/<path> (the authoritative edit location)
 * When the source changes it is first copied over the output (atomic write),
 * then both converge on the same change detection. Editing either side triggers
 * a reload. A file seen for the first time is only seeded (no spurious reload).
 */
class RmlUIHotReloadService
{
public:
	RmlUIHotReloadService();

	/// Enable/disable polling. Enabled by default.
	void Enable(bool enabled) { m_enabled = enabled; }

	/// Register a loaded document path (relative, e.g. "RmlUI/DemoPanel.rml").
	/// Seeds the mtime stamps and remembers the directory to scan.
	void TrackPath(const String& rel_path);

	/// Accumulate dt and poll at a fixed interval. Empty set == no changes.
	UIHotReloadChangeSet Poll(Float32 dt);

private:
	static constexpr Float32 kPollInterval = 0.5f;
	static constexpr UInt32  kMaxAncestorLevels = 8;

	/// Per-file mtimes of both copies (0.0 = file absent).
	struct FileStamp { Float64 source = 0.0; Float64 output = 0.0; };

	/// Walk up from cwd looking for a directory containing resource/RmlUI.
	/// Returns the project root ("" if not found — then only the output is watched).
	static String FindProjectResourceRoot();

	/// Scan one tracked dir for *.rml/*.rcss, detect changes, copy source→output.
	void ScanDirectory(const String& dir, UIHotReloadChangeSet& out);

	Map<String, FileStamp> m_stamps;      // rel path → stamps ("" dir = cwd root)
	Vector<String> m_tracked_dirs;        // unique dirs, derived from TrackPath
	String m_source_root;                 // project root; "" = not found
	Float32 m_timer = 0.0f;
	bool m_enabled = true;
};

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RMLUIHOTRELOADSERVICE_
