import 'package:flutter/material.dart';
import 'package:get_it/get_it.dart';

import '../../services/media_player.dart';
import '../../utils/adaptive_widgets/icons.dart';

/// Global floating volume control, mounted once above the whole app (see
/// [Echo.build] in main.dart) so it stays available on every screen,
/// including the full-screen player route.
class FloatingVolumeControl extends StatefulWidget {
  const FloatingVolumeControl({super.key});

  @override
  State<FloatingVolumeControl> createState() => _FloatingVolumeControlState();
}

class _FloatingVolumeControlState extends State<FloatingVolumeControl> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final player = GetIt.I<MediaPlayer>().player;
    return Stack(
      children: [
        if (_expanded)
          Positioned.fill(
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: () => setState(() => _expanded = false),
              child: Container(color: Colors.transparent),
            ),
          ),
        Positioned(
          top: 16,
          right: 16,
          child: StreamBuilder<double>(
            stream: player.volumeStream,
            initialData: player.volume,
            builder: (context, snapshot) {
              final volume = snapshot.data ?? player.volume;
              return _expanded
                  ? Material(
                      elevation: 4,
                      borderRadius: BorderRadius.circular(24),
                      color: Theme.of(context).colorScheme.surfaceContainerHigh,
                      child: SizedBox(
                        width: 48,
                        height: 180,
                        child: Column(
                          children: [
                            IconButton(
                              icon: Icon(AdaptiveIcons.volume(volume)),
                              onPressed: () => setState(() => _expanded = false),
                            ),
                            Expanded(
                              child: RotatedBox(
                                quarterTurns: 3,
                                child: Slider(
                                  value: volume,
                                  onChanged: player.setVolume,
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                    )
                  : FloatingActionButton.small(
                      heroTag: 'volume',
                      onPressed: () => setState(() => _expanded = true),
                      child: Icon(AdaptiveIcons.volume(volume)),
                    );
            },
          ),
        ),
      ],
    );
  }
}
