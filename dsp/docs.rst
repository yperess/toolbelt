.. _module-dsp:

===
dsp
===

``dsp`` provides async-friendly signal-processing utilities built on
``pw_async2``. All debounce implementations are ``pw::async2::Task`` objects:
post them to a dispatcher and consume state transitions through the
``NextState()`` future API.

--------
Debounce
--------
All implementations extend the ``Debounce`` base class and share the same
consumer API.

Consuming state changes
=======================
``Debounce::NextState()`` returns a one-shot ``pw::async2::Future<State>``
that resolves to the next ``kActive`` or ``kInactive`` transition. Call it
again inside a loop to observe subsequent transitions.

.. tab-set::

   .. tab-item:: Standard polling

      .. code-block:: cpp

         class ButtonTask : public pw::async2::Task {
          public:
           explicit ButtonTask(pw::dsp::Debounce& debouncer) : debouncer_(debouncer) {}

          private:
           pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
             while (true) {
               auto poll = debouncer_.NextState().Pend(cx);
               if (poll.IsPending())
                 return pw::async2::Pending();
               HandleChange(*poll);  // *poll is Debounce::State
             }
           }

           pw::dsp::Debounce& debouncer_;
         };

   .. tab-item:: C++20 coroutines

      .. code-block:: cpp

         pw::async2::Coro<void> MonitorButton(pw::async2::CoroContext cx,
                                              pw::dsp::Debounce& debouncer) {
           while (true) {
             HandleChange(co_await debouncer.NextState());
           }
         }

PollingDebounce
===============
``PollingDebounce`` samples a callable at a fixed interval and requires
``stable_count`` consecutive identical readings before emitting a state
change. Use this when the signal source does not provide interrupts.

.. code-block:: cpp

   pw::dsp::PollingDebounce btn(
       []() { return ReadPin(); },  // returns Debounce::State
       time_provider,
       /*poll_interval=*/10ms,
       /*stable_count=*/5);
   dispatcher.Post(btn);

.. tab-set::

   .. tab-item:: Standard polling

      .. code-block:: cpp

         class ButtonTask : public pw::async2::Task {
          public:
           explicit ButtonTask(pw::dsp::PollingDebounce& btn) : btn_(btn) {}

          private:
           pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
             while (true) {
               auto poll = btn_.NextState().Pend(cx);
               if (poll.IsPending()) {
                 return pw::async2::Pending();
               }
               if (*poll == pw::dsp::Debounce::State::kActive) {
                 OnPress();
               } else {
                 OnRelease();
               }
             }
           }

           pw::dsp::PollingDebounce& btn_;
         };

   .. tab-item:: C++20 coroutines

      .. code-block:: cpp

         pw::async2::Coro<void> MonitorButton(pw::async2::CoroContext cx,
                                              pw::dsp::PollingDebounce& btn) {
           while (true) {
             if (co_await btn.NextState() == pw::dsp::Debounce::State::kActive) {
               OnPress();
             } else {
               OnRelease();
             }
           }
         }

Interrupt-driven debouncers
===========================
``LeadingEdgeDebounce`` and ``PulseWidthDebounce`` both extend
``InterruptBasedDebounce``, which exposes ``OnInterrupt()`` as the common
entry point for the GPIO interrupt handler.

Because ``OnInterrupt()`` is on the shared base class, a single interrupt
source can be fanned out to multiple debouncers with no coupling between
the wiring logic and the concrete debouncer types:

.. code-block:: cpp

   pw::dsp::LeadingEdgeDebounce tap(time, std::chrono::milliseconds(50));
   pw::dsp::PulseWidthDebounce long_press(time, std::chrono::seconds(2));

   io.SetInterruptHandler(pw::digital_io::InterruptTrigger::kBothEdges,
                          [](pw::digital_io::State s) {
                            tap.OnInterrupt(s);
                            long_press.OnInterrupt(s);
                          });
   io.EnableInterruptHandler();

   dispatcher.Post(tap);
   dispatcher.Post(long_press);

Or, using the base-class pointer for generic wiring code:

.. code-block:: cpp

   pw::dsp::InterruptBasedDebounce* debouncers[] = {&tap, &long_press};

   io.SetInterruptHandler(pw::digital_io::InterruptTrigger::kBothEdges,
                          [](pw::digital_io::State s) {
                            for (auto* d : debouncers) {
                              d->OnInterrupt(s);
                            }
                          });

LeadingEdgeDebounce
-------------------
Fires ``kActive`` immediately on the first rising edge, then opens a
debounce window (``threshold``) during which all further interrupts are
ignored. After the window, waits for the falling edge and fires
``kInactive``.

Use this for tap or click detection: the ``kActive`` notification arrives
before any contact bounce can occur, giving minimum response latency.

.. wavedrom::

   { "signal": [
       { "name": "pin (state)",
         "wave": "010101.....0.",
         "node": ".A.........C." },
       { "name": "debounce window",
         "wave": "01.......0...",
         "node": ".D.......E..." },
       { "name": "NextState()",
         "wave": "x=.........=.",
         "data": ["kActive", "kInactive"] }
     ],
     "edge": ["D<->E threshold"],
     "config": { "hscale": 1 }
   }

.. tab-set::

   .. tab-item:: Standard polling

      .. code-block:: cpp

         pw::dsp::LeadingEdgeDebounce tap(time, std::chrono::milliseconds(50));
         dispatcher.Post(tap);

         class TapTask : public pw::async2::Task {
          public:
           explicit TapTask(pw::dsp::LeadingEdgeDebounce& tap) : tap_(tap) {}

          private:
           pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
             while (true) {
               auto poll = tap_.NextState().Pend(cx);
               if (poll.IsPending()) {
                 return pw::async2::Pending();
               }
               if (*poll == pw::dsp::Debounce::State::kActive) {
                 OnTap();
               }
             }
           }

           pw::dsp::LeadingEdgeDebounce& tap_;
         };

   .. tab-item:: C++20 coroutines

      .. code-block:: cpp

         pw::dsp::LeadingEdgeDebounce tap(time, std::chrono::milliseconds(50));
         dispatcher.Post(tap);

         pw::async2::Coro<void> MonitorTap(pw::async2::CoroContext cx,
                                           pw::dsp::LeadingEdgeDebounce& tap) {
           while (true) {
             if (co_await tap.NextState() == pw::dsp::Debounce::State::kActive) {
               OnTap();
             }
           }
         }

PulseWidthDebounce
------------------
Starts a timer on the rising edge. If the signal is continuously active when
the timer expires, fires ``kActive``; the subsequent release fires
``kInactive``. Presses shorter than ``threshold`` produce no notification and
are discarded as noise.

Use this for long-press or deliberate-hold detection (e.g. a 5 s
factory-reset gesture).

.. wavedrom::

   { "signal": [
       { "name": "pin",
         "wave": "01.....0....",
         "node": ".A.....B...." },
       { "name": "hold timer",
         "wave": "0=.....x....",
         "node": ".C.........D",
         "data": [""] },
       { "name": "NextState()",
         "wave": "x..........." }
     ],
     "edge": ["C<->D threshold"],
     "head": { "text": "Short press — released before threshold, no notification" },
     "config": { "hscale": 1 }
   }

.. wavedrom::

   { "signal": [
       { "name": "pin",
         "wave": "01........0.",
         "node": ".A........B." },
       { "name": "hold timer",
         "wave": "0=....=.x0..",
         "node": ".C....D.....",
         "data": ["", "expired"] },
       { "name": "NextState()",
         "wave": "x.....=...=.",
         "data": ["kActive", "kInactive"] }
     ],
     "edge": ["C<~>D threshold"],
     "head": { "text": "Long press — held ≥ threshold → kActive then kInactive on release" },
     "config": { "hscale": 1 }
   }

.. tab-set::

   .. tab-item:: Standard polling

      .. code-block:: cpp

         pw::dsp::PulseWidthDebounce long_press(time, 5s);
         dispatcher.Post(long_press);

         class LongPressTask : public pw::async2::Task {
          public:
           explicit LongPressTask(pw::dsp::PulseWidthDebounce& btn) : btn_(btn) {}

          private:
           pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
             while (true) {
               auto poll = btn_.NextState().Pend(cx);
               if (poll.IsPending()) {
                 return pw::async2::Pending();
               }
               if (*poll == pw::dsp::Debounce::State::kActive) {
                 OnLongPress();
               }
             }
           }

           pw::dsp::PulseWidthDebounce& btn_;
         };

   .. tab-item:: C++20 coroutines

      .. code-block:: cpp

         pw::dsp::PulseWidthDebounce long_press(time, 5s);
         dispatcher.Post(long_press);

         pw::async2::Coro<void> MonitorLongPress(pw::async2::CoroContext cx,
                                                 pw::dsp::PulseWidthDebounce& btn) {
           while (true) {
             if (co_await btn.NextState() == pw::dsp::Debounce::State::kActive) {
               OnLongPress();
             }
           }
         }
