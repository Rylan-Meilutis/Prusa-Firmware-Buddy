# Prusa-Firmware-Buddy Release Notes Style

## Title
Format: `X.Y.Z Firmware for Prusa <printer list>`
Pre-releases append `-alpha`, `-RC`, `-RC2` etc. to the version.

## Structure

### 1. Optional urgent notice (H1 `# Update`)
Used only when a release has a critical issue (e.g. withdrawn for a specific printer).
Written in plain prose, bold emphasis on key terms, actionable instructions.

### 2. Summary (H1 `# Summary`)
Bulleted list grouped into categories:
- **New features** — bold heading, one bullet per feature
- **Changes and improvements** — bold heading, one bullet per change
- **Fixes** — bold heading, one bullet per fix

Further points:
- Each bullet is a short noun phrase or sentence fragment (no trailing period).
- Printer tags and GitHub issue references go at the **end** of the bullet/title, not at the beginning.
- Printer-specific items tagged with `[XL]`, `[C1]`, `[C1/+]`, `[MK4 family]` etc. MMU-specific items tagged with `[MMU]`.
- GitHub issue references inline as `(#1234)`.
- Smaller/patch releases may skip the categorized grouping and just list 3-5 bullets directly.

### 3. Context paragraph
One paragraph placed between the summary and the feature sections. It follows a consistent opening pattern:

> This is the **stable release** of firmware **X.Y.Z** for the **(printer list)**, bringing (brief characterization of changes).

Variants of the opener:
- Stable: `"This is the stable release of firmware 6.4.0 for the CORE One+, CORE One, MK4S, ... and XL."`
- RC: `"This is the (second/nth) release candidate for the upcoming firmware 6.4.0 for the Prusa CORE One, MK4S, ..."`
- Introductory (new printer): `"This release introduces support for the new Prusa CORE One L and its unique features, including ..."`

The opener is followed by a clause describing the release's purpose, connected with a comma, dash, or period:
- `", bringing targeted stability improvements and fixes."`
- `", addressing several improvements and bug fixes, most notably ..."`
- `"– along with bringing new features, quality-of-life improvements, and fixes across all platforms."`

When relevant, the paragraph also explains broader context such as branch reunification or development history (e.g. "It reunifies the firmware development branch after the temporary split during the CORE One launch, where 6.3.x was released exclusively for CORE One and other printers remained on 6.2.x.").

For pre-releases, the paragraph ends with a sentence linking back to earlier alpha/RC notes:
> For a complete overview of features introduced in firmware 6.4.0, users can refer to the 6.4.0-RC release notes published earlier.

### 4. Detail sections (H2)
Each notable change — whether a new feature, improvement, or fix — gets its own H2 section. There is no structural distinction between them. Each contains:
- **Title**: Same title as in the summary, including printer tags and github references.
- **Prose description**: 1-3 paragraphs, written for end users. Explains *what* changed, *why*, and *how to use it*. References menu paths in bold (`**Settings → Hardware → ...**`). Mentions relevant G-codes with syntax examples.
- **Notes/caveats**: Italicized `_NOTE: ..._` blocks at the end of sections.

### 6. Known issues (H2)
Same bulleted list structure as Remaining fixes, but describing known regressions. Ends with a note that they'll be addressed in the final release.

## Tone & conventions
- Written for end users, not developers. Never mention internal function names, variable names, code constructs, or implementation details. Describe what the user would observe (the symptom and the resolution), not how the code was changed.
- Explanatory but not verbose — each section is self-contained.
- Bold for UI paths, printer models, key terms.
- Italics for notes and caveats.
- Printer tags in square brackets: `[XL]`, `[C1]`, `[C1/+]`, `[C1L]`.
- GitHub issues referenced as `(#NNNN)`.
- No emojis. No exclamation marks. Professional, calm register.
- Do not mention iX and iX-only changes, iX goes through different release channels.

## Instructions for the AI
- Notify the user if you think some change warrants a screenshot to be added.
- For review purposes (will be removed before the actual release), add links to all relevant tickets in the change bullets ONLY in the summary. It should be a link in the https://dev.prusa3d.com/browse/BFW-XXXX format. The relevant tickets should always be present in each commit message. Warn if there is a commit without a BFW.
- Put the output to the "CHANGELOG-X.X.X.md" file.
- All releases have an appropriate `vX.Y.Z` git tag. When tasked to do release notes for some new version, make it for the diff between the head `RELEASE-X.Y` branch (make sure it's up to date please) and the appropriate tag.
- Do not mention that the translations have been updated - that's implicit for every release.
- Don't mention dependent firmware version bumps, except for the bootloader and MMU.
    - If bootloader gets updated, prompt the user to explain the changes.
    - For the MMU, the users need to update the FW themselves.
- Check the changed code as well, not only the commit messages. They can be misleading.
- Do not mention rewordings
- Order the changes from the most impactfull to the least
- Please keep in mind that some of the commits/bugfixes are bugfixes to things introduced when adding the new features for the same version. Those bugfixes should not be mentioned.

Final checklist:
- Check that titles match with the summary
- Check descriptions match the titles
