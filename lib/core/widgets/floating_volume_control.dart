import 'package:flutter/material.dart';
import 'package:get_it/get_it.dart';

import '../../services/media_player.dart';
import '../../utils/adaptive_widgets/icons.dart';

/// Global floating volume control, mounted once above the whole app (see
/// [Echo.build] in main.dart) so it stays available on every screen,
/// including the full-screen player route. Draggable to any corner/edge.
class FloatingVolumeControl extends StatefulWidget {
  const FloatingVolumeControl({super.key});

  @override
  State<FloatingVolumeControl> createState() => _FloatingVolumeControlState();
}

class _FloatingVolumeControlState extends State<FloatingVolumeControl> {
  bool _expanded = false;
  double _top = 16;
  double _right = 16;

  static const double _width = 48;

  void _drag(DragUpdateDetails details, Size bounds, double height) {
    setState(() {
      _top = (_top + details.delta.dy).clamp(0, bounds.height - height);
      _right = (_right - details.delta.dx).clamp(0, bounds.width - _width);
    });
  }

  @override
  Widget build(BuildContext context) {
    final player = GetIt.I<MediaPlayer>().player;
    final height = _expanded ? 180.0 : _width;
    return LayoutBuilder(
      builder: (context, constraints) {
        final bounds = constraints.biggest;
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
              top: _top,
              right: _right,
              child: StreamBuilder<double>(
                stream: player.volumeStream,
                initialData: player.volume,
                builder: (context, snapshot) {
                  final volume = snapshot.data ?? player.volume;
                  // Only the collapsed button is draggable; while expanded the
                  // gesture area must stay free for the slider's own drag.
                  return _expanded
                      ? Material(
                          elevation: 4,
                          borderRadius: BorderRadius.circular(24),
                          color:
                              Theme.of(context).colorScheme.surfaceContainerHigh,
                          child: SizedBox(
                            width: _width,
                            height: height,
                            child: Column(
                              children: [
                                IconButton(
                                  icon: Icon(AdaptiveIcons.volume(volume)),
                                  onPressed: () =>
                                      setState(() => _expanded = false),
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
                      : GestureDetector(
                          onPanUpdate: (details) =>
                              _drag(details, bounds, height),
                          child: FloatingActionButton.small(
                            heroTag: 'volume',
                            onPressed: () => setState(() => _expanded = true),
                            child: Icon(AdaptiveIcons.volume(volume)),
                          ),
                        );
                },
              ),
            ),
          ],
        );
      },
    );
  }
}
