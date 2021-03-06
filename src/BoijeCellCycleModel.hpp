#ifndef BOIJECELLCYCLEMODEL_HPP_
#define BOIJECELLCYCLEMODEL_HPP_

#include "AbstractSimpleCellCycleModel.hpp"
#include "RandomNumberGenerator.hpp"
#include "Cell.hpp"
#include "DifferentiatedCellProliferativeType.hpp"
#include "SmartPointers.hpp"
#include "ColumnDataWriter.hpp"
#include "LogFile.hpp"
#include "CellLabel.hpp"

#include "BoijeRetinalNeuralFates.hpp"


/*******************************
* BOIJE CELL CYCLE MODEL
* As described in Boije et al. 2015 [Boije2015] doi: 10.1016/j.devcel.2015.08.011
*
* USE: By default, BoijeCellCycleModels are constructed with the parameters reported in [Boije2015].
* In normal use, the model steps through three phases of probabilistic transcription factor signalling.
* These signals determine the mitotic mode and the fate of offspring.
*
* NB: the Boije model is purely generational and assumes a generation time of 1; time in Boije simulations
* thus represents generation number rather than proper time. Refactor for use with simulations that refer
* to clocktime!!
 *
 * 2 per-model-event output modes:
 * EnableModeEventOutput() enables mitotic mode event logging-all cells will write to the singleton log file
 * EnableModelDebugOutput() enables more detailed debug output, each seed will have its own file written to
 * by a ColumnDataWriter passed to it from the test
 * (eg. by the SetupDebugOutput helper function in the project simulator)
 *
 * 1 mitotic-event-sequence sampler (only samples one "path" through the lineage):
 * EnableSequenceSampler() - one "sequence" of progenitors writes mitotic event type to a string in the singleton log file
 *
 *********************************/

class BoijeCellCycleModel : public AbstractSimpleCellCycleModel
{
    friend class TestSimpleCellCycleModels;

private:

    /** Needed for serialization. */
    friend class boost::serialization::access;
    /**
     * Archive the cell-cycle model and random number generator, never used directly - boost uses this.
     *
     * @param archive the archive
     * @param version the current version of this class
     */
    template<class Archive>
    void serialize(Archive & archive, const unsigned int version)
    {
        archive & boost::serialization::base_object<AbstractSimpleCellCycleModel>(*this);

        SerializableSingleton<RandomNumberGenerator>* p_wrapper = RandomNumberGenerator::Instance()->GetSerializationWrapper();
        archive & p_wrapper;
        archive & mCellCycleDuration;
        archive & mGeneration;
    }

    //Private write functions for models
    void WriteModeEventOutput();
    void WriteDebugData(double atoh7RV, double ptf1aRV, double ngRV);

protected:
    //mode/output variables
    bool mOutput;
    double mEventStartTime;
    bool mSequenceSampler;
    bool mSeqSamplerLabelSister;
    //debug writer stuff
    bool mDebug;
    int mTimeID;
    std::vector<int> mVarIDs;
    boost::shared_ptr<ColumnDataWriter> mDebugWriter;
    //model parameters and state memory vars
    unsigned mGeneration;
    unsigned mPhase2gen;
    unsigned mPhase3gen;
    double mprobAtoh7;
	double mprobPtf1a;
	double mprobng;
	bool mAtoh7Signal;
	bool mPtf1aSignal;
	bool mNgSignal;
    unsigned mMitoticMode;
    unsigned mSeed;
    boost::shared_ptr<AbstractCellProperty> mp_PostMitoticType;
    boost::shared_ptr<AbstractCellProperty> mp_RGC_Type;
    boost::shared_ptr<AbstractCellProperty> mp_AC_HC_Type;
    boost::shared_ptr<AbstractCellProperty> mp_PR_BC_Type;
    boost::shared_ptr<AbstractCellProperty> mp_label_Type;

    /**
     * Protected copy-constructor for use by CreateCellCycleModel().
     *
     * The only way for external code to create a copy of a cell cycle model
     * is by calling that method, to ensure that a model of the correct subclass is created.
     * This copy-constructor helps subclasses to ensure that all member variables are correctly copied when this happens.
     *
     * This method is called by child classes to set member variables for a daughter cell upon cell division.
     * Note that the parent cell cycle model will have had ResetForDivision() called just before CreateCellCycleModel() is called,
     * so performing an exact copy of the parent is suitable behaviour. Any daughter-cell-specific initialisation
     * can be done in InitialiseDaughterCell().
     *
     * @param rModel the cell cycle model to copy.
     */
    BoijeCellCycleModel(const BoijeCellCycleModel& rModel);

public:

    /**
     * Constructor - just a default, mBirthTime is set in the AbstractCellCycleModel class.
     */
    BoijeCellCycleModel();

    /**
     * SetCellCycleDuration() method to set length of cell cycle (default 1.0 as Boije's model is purely generational)
     */
    void SetCellCycleDuration();

    /**
     * Overridden builder method to create new copies of
     * this cell-cycle model.
     *
     * @return new cell-cycle model
     */
    AbstractCellCycleModel* CreateCellCycleModel();
    
    /**
     * Overridden ResetForDivision() method.
     * Contains general mitotic mode logic
     **/
    void ResetForDivision();
    
    /**
     * Overridden InitialiseDaughterCell() method.
     * Used to implement asymmetric mitotic mode
     * */
    void InitialiseDaughterCell();

    /**
     * Sets the cell's generation.
     *
     * @param generation the cell's generation
     **/
    void SetGeneration(unsigned generation);

    /**
     * @return the cell's generation.
     */
    unsigned GetGeneration() const;  
    
    /*****************************
     * Model setup functions:
     * Set TF signal probabilities with SetModelParameters
     * Set Postmitotic and specification AbstractCellProperties w/ the relevant functions
     * By default these are available in BoijeRetinalNeuralFates.hpp
     *
     * NB: The Boije model lumps together amacrine & horizontal cells, photoreceptors & bipolar neurons
     ******************************/

    void SetModelParameters(unsigned phase2gen = 3, unsigned phase3gen = 5, double probAtoh7 = 0.32, double probPtf1a = 0.3, double probng = 0.8);
    void SetPostMitoticType(boost::shared_ptr<AbstractCellProperty> p_PostMitoticType);
    void SetSpecifiedTypes(boost::shared_ptr<AbstractCellProperty> p_RGC_Type, boost::shared_ptr<AbstractCellProperty> p_AC_HC_Type, boost::shared_ptr<AbstractCellProperty> p_PR_BC_Type);
    
    //Functions to enable per-cell mitotic mode logging for mode rate & sequence sampling fixtures
    //Uses singleton logfile
    void EnableModeEventOutput(double eventStart, unsigned seed);
    void EnableSequenceSampler(boost::shared_ptr<AbstractCellProperty> label);

    //More detailed debug output. Needs a ColumnDataWriter passed to it
    //Only declare ColumnDataWriter directory, filename, etc; do not set up otherwise
    void EnableModelDebugOutput(boost::shared_ptr<ColumnDataWriter> debugWriter);

    /**
     * Overridden GetAverageTransitCellCycleTime() method.
     *
     * @return the average of mMinCellCycleDuration and mMaxCellCycleDuration
     */
    double GetAverageTransitCellCycleTime();

    /**
     * Overridden GetAverageStemCellCycleTime() method.
     *
     * @return the average of mMinCellCycleDuration and mMaxCellCycleDuration
     */
    double GetAverageStemCellCycleTime();

    /**
     * Overridden OutputCellCycleModelParameters() method.
     *
     * @param rParamsFile the file stream to which the parameters are output
     */
    virtual void OutputCellCycleModelParameters(out_stream& rParamsFile);
};

#include "SerializationExportWrapper.hpp"
// Declare identifier for the serializer
CHASTE_CLASS_EXPORT(BoijeCellCycleModel)

#endif /*BOIJECELLCYCLEMODEL_HPP_*/
