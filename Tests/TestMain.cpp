#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Esegue tutti i juce::UnitTest registrati; codice di uscita 1 se almeno un test fallisce.
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    juce::String onlyCategory;
    if (argc > 1)
        onlyCategory = argv[1];

    if (onlyCategory.isNotEmpty())
        runner.runTestsInCategory (onlyCategory);
    else
        runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::printf ("\n%s: %d test group(s), %d failure(s)\n",
                 failures == 0 ? "ALL PASSED" : "FAILED",
                 runner.getNumResults(), failures);
    return failures == 0 ? 0 : 1;
}
