# ITGmania 0.7.1 (FA+)

based on ITGmania 0.9.0-beta, rolled back to 0.7.0 & SL 5.3.0,  but kept all sync fixes.
last updated 8/16/2024

itgm 0.9.0+ themes will work on this build but you may need to trick the theme into allowing you to use the version number 0.7.1, in simply love this is Scripts/SL-SupportHelpers.lua

```
	-- ITGmania >= 0.8.0
	if IsITGmania() then
		return IsMinimumProductVersion(0, 7, 0)
```
