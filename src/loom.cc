#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <distributions/random.hpp>
#include <distributions/clustering.hpp>
#include <distributions/models/dd.hpp>
#include <distributions/models/dpd.hpp>
#include <distributions/models/nich.hpp>
#include <distributions/models/gp.hpp>
#include <distributions/protobuf.hpp>
#include "common.hpp"
#include "protobuf.hpp"

#define TODO(message) LOOM_ERROR("TODO " << message)

namespace loom
{

using distributions::rng_t;
using distributions::VectorFloat;


struct ProductModel
{
    typedef distributions::Clustering<int>::PitmanYor Clustering;

    Clustering clustering;
    std::vector<distributions::DirichletDiscrete<16>> dd;
    std::vector<distributions::DirichletProcessDiscrete> dpd;
    std::vector<distributions::GammaPoisson> gp;
    std::vector<distributions::NormalInverseChiSq> nich;

    void load (const char * filename);
};

void ProductModel::load (const char * filename)
{
    std::fstream file(filename, std::ios::in | std::ios::binary);
    LOOM_ASSERT(file, "models file not found");
    protobuf::ProductModel product_model;
    bool info = product_model.ParseFromIstream(&file);
    LOOM_ASSERT(info, "failed to parse model from file");

    clustering.alpha = product_model.clustering().pitman_yor().alpha();
    clustering.d = product_model.clustering().pitman_yor().d();

    for (size_t i = 0; i < product_model.bb_size(); ++i) {
        TODO("load bb models");
    }

    dd.resize(product_model.dd_size());
    for (size_t i = 0; i < product_model.dd_size(); ++i) {
        distributions::model_load(dd[i], product_model.dd(i));
    }

    dpd.resize(product_model.dpd_size());
    for (size_t i = 0; i < product_model.dpd_size(); ++i) {
        distributions::model_load(dpd[i], product_model.dpd(i));
    }

    gp.resize(product_model.gp_size());
    for (size_t i = 0; i < product_model.gp_size(); ++i) {
        distributions::model_load(gp[i], product_model.gp(i));
    }

    nich.resize(product_model.nich_size());
    for (size_t i = 0; i < product_model.nich_size(); ++i) {
        distributions::model_load(nich[i], product_model.nich(i));
    }
}


struct ProductMixture
{
    typedef protobuf::ProductModel::SparseValue Value;

    const ProductModel & model;
    size_t empty_groupid;
    ProductModel::Clustering::Mixture clustering;
    std::vector<distributions::DirichletDiscrete<16>::Classifier> dd;
    std::vector<distributions::DirichletProcessDiscrete::Classifier> dpd;
    std::vector<distributions::GammaPoisson::Classifier> gp;
    std::vector<distributions::NormalInverseChiSq::Classifier> nich;

    ProductMixture (const ProductModel & m) : model(m) {}

    void init (rng_t & rng);
    void add_group (rng_t & rng);
    void remove_group (size_t groupid);
    void add_value (size_t groupid, const Value & value, rng_t & rng);
    void remove_value (size_t groupid, const Value & value, rng_t & rng);
    void score (const Value & value, VectorFloat & scores, rng_t & rng);

    void load (const char * filename) { TODO("load"); }
    void dump (const char * filename);

private:

    template<class Model>
    void init_factors (
            const std::vector<Model> & models,
            std::vector<typename Model::Classifier> & mixtures,
            rng_t & rng);

    template<class Fun>
    void apply_dense (Fun & fun);

    template<class Fun>
    void apply_sparse (Fun & fun, const Value & value);

    struct score_fun;
    struct add_group_fun;
    struct remove_group_fun;
    struct add_value_fun;
    struct remove_value_fun;
    struct dump_group_fun;
};

template<class Model>
void ProductMixture::init_factors (
        const std::vector<Model> & models,
        std::vector<typename Model::Classifier> & mixtures,
        rng_t & rng)
{
    const size_t count = models.size();
    mixtures.clear();
    mixtures.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const auto & model = models[i];
        auto & mixture = mixtures[i];
        mixture.groups.resize(1);
        model.group_init(mixture.groups[0], rng);
        model.classifier_init(mixture, rng);
    }
}

void ProductMixture::init (rng_t & rng)
{
    empty_groupid = 0;

    clustering.counts.resize(1);
    clustering.counts[0] = 0;
    model.clustering.mixture_init(clustering);

    init_factors(model.dd, dd, rng);
    init_factors(model.dpd, dpd, rng);
    init_factors(model.gp, gp, rng);
    init_factors(model.nich, nich, rng);
}

template<class Fun>
inline void ProductMixture::apply_dense (Fun & fun)
{
    //TODO("implement bb");
    for (size_t i = 0; i < dd.size(); ++i) {
        fun(model.dd[i], dd[i]);
    }
    for (size_t i = 0; i < dpd.size(); ++i) {
        fun(model.dpd[i], dpd[i]);
    }
    for (size_t i = 0; i < gp.size(); ++i) {
        fun(model.gp[i], gp[i]);
    }
    for (size_t i = 0; i < nich.size(); ++i) {
        fun(model.nich[i], nich[i]);
    }
}

template<class Fun>
inline void ProductMixture::apply_sparse (Fun & fun, const Value & value)
{
    size_t observed_pos = 0;

    if (value.booleans_size()) {
        TODO("implement bb");
    }

    if (value.counts_size()) {
        size_t data_pos = 0;
        for (size_t i = 0; i < dd.size(); ++i) {
            if (value.observed(observed_pos++)) {
                fun(model.dd[i], dd[i], value.counts(data_pos++));
            }
        }
        for (size_t i = 0; i < dpd.size(); ++i) {
            if (value.observed(observed_pos++)) {
                fun(model.dpd[i], dpd[i], value.counts(data_pos++));
            }
        }
        for (size_t i = 0; i < gp.size(); ++i) {
            if (value.observed(observed_pos++)) {
                fun(model.gp[i], gp[i], value.counts(data_pos++));
            }
        }
    }

    if (value.reals_size()) {
        size_t data_pos = 0;
        for (size_t i = 0; i < nich.size(); ++i) {
            if (value.observed(observed_pos++)) {
                fun(model.nich[i], nich[i], value.reals(data_pos++));
            }
        }
    }
}

struct ProductMixture::add_group_fun
{
    rng_t & rng;

    template<class Model>
    void operator() (
            const Model & model,
            typename Model::Classifier & mixture)
    {
        model.classifier_add_group(mixture, rng);
    }
};

inline void ProductMixture::add_group (rng_t & rng)
{
    model.clustering.mixture_add_group(clustering);
    add_group_fun fun = {rng};
    apply_dense(fun);
}

struct ProductMixture::remove_group_fun
{
    const size_t groupid;

    template<class Model>
    void operator() (
            const Model & model,
            typename Model::Classifier & mixture)
    {
        model.classifier_remove_group(mixture, groupid);
    }
};

inline void ProductMixture::remove_group (size_t groupid)
{
    LOOM_ASSERT2(groupid != empty_groupid, "cannot remove empty group");
    if (empty_groupid == clustering.counts.size() - 1) {
        empty_groupid = groupid;
    }

    model.clustering.mixture_remove_group(clustering, groupid);
    remove_group_fun fun = {groupid};
    apply_dense(fun);
}

struct ProductMixture::add_value_fun
{
    const size_t groupid;
    rng_t & rng;

    template<class Model>
    void operator() (
        const Model & model,
        typename Model::Classifier & mixture,
        const typename Model::Value & value)
    {
        model.classifier_add_value(mixture, groupid, value, rng);
    }
};

inline void ProductMixture::add_value (
        size_t groupid,
        const Value & value,
        rng_t & rng)
{
    if (unlikely(groupid == empty_groupid)) {
        empty_groupid = clustering.counts.size();
        add_group(rng);
    }

    model.clustering.mixture_add_value(clustering, groupid);
    add_value_fun fun = {groupid, rng};
    apply_sparse(fun, value);
}

struct ProductMixture::remove_value_fun
{
    const size_t groupid;
    rng_t & rng;

    template<class Model>
    void operator() (
        const Model & model,
        typename Model::Classifier & mixture,
        const typename Model::Value & value)
    {
        model.classifier_remove_value(mixture, groupid, value, rng);
    }
};

inline void ProductMixture::remove_value (
        size_t groupid,
        const Value & value,
        rng_t & rng)
{
    LOOM_ASSERT2(groupid != empty_groupid, "cannot remove empty group");

    model.clustering.mixture_remove_value(clustering, groupid);
    remove_value_fun fun = {groupid, rng};
    apply_sparse(fun, value);

    if (unlikely(clustering.counts[groupid] == 0)) {
        remove_group(groupid);
    }
}

struct ProductMixture::score_fun
{
    VectorFloat & scores;
    rng_t & rng;

    template<class Model>
    void operator() (
        const Model & model,
        const typename Model::Classifier & mixture,
        const typename Model::Value & value)
    {
        model.classifier_score(mixture, value, scores, rng);
    }
};

inline void ProductMixture::score (
        const Value & value,
        VectorFloat & scores,
        rng_t & rng)
{
    model.clustering.mixture_score(clustering, scores);
    score_fun fun = {scores, rng};
    apply_sparse(fun, value);
}

struct ProductMixture::dump_group_fun
{
    size_t groupid;
    protobuf::ProductModel::Group & message;

    void operator() (
            const distributions::DirichletDiscrete<16> & model,
            const distributions::DirichletDiscrete<16>::Classifier & mixture)
    {
        distributions::group_dump(
                model,
                mixture.groups[groupid],
                * message.add_dd());
    }

    void operator() (
            const distributions::DirichletProcessDiscrete & model,
            const distributions::DirichletProcessDiscrete::Classifier &
                mixture)
    {
        distributions::group_dump(
                model,
                mixture.groups[groupid],
                * message.add_dpd());
    }

    void operator() (
            const distributions::GammaPoisson & model,
            const distributions::GammaPoisson::Classifier & mixture)
    {
        distributions::group_dump(
                model,
                mixture.groups[groupid],
                * message.add_gp());
    }

    void operator() (
            const distributions::NormalInverseChiSq & model,
            const distributions::NormalInverseChiSq::Classifier & mixture)
    {
        distributions::group_dump(
                model,
                mixture.groups[groupid],
                * message.add_nich());
    }
};

void ProductMixture::dump (const char * filename)
{
    loom::protobuf::OutFileStream groups_stream(filename);
    protobuf::ProductModel::Group message;
    const size_t group_count = clustering.counts.size();
    for (size_t i = 0; i < group_count; ++i) {
        dump_group_fun fun = {i, message};
        apply_dense(fun);
        groups_stream.write(message);
        message.Clear();
    }
}

} // namespace loom


const char * help_message =
"Usage: loom MODEL_IN VALUES_IN GROUPS_OUT"
;


int main (int argc, char ** argv)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (argc != 4) {
        std::cerr << help_message << std::endl;
        exit(1);
    }

    const char * model_in = argv[2];
    const char * values_in = argv[3];
    const char * groups_out = argv[4];

    distributions::rng_t rng;

    loom::ProductModel model;
    model.load(model_in);

    loom::ProductMixture mixture(model);
    mixture.init(rng);
    //mixture.load(groups_in);

    {
        loom::ProductMixture::Value value;
        distributions::VectorFloat scores;
        loom::protobuf::InFileStream values_stream(values_in);
        while (values_stream.try_read(value)) {
            mixture.score(value, scores, rng);
            size_t groupid = distributions::sample_discrete(
                rng,
                scores.size(),
                scores.data());
            mixture.add_value(groupid, value, rng);
        }
    }

    mixture.dump(groups_out);

    return 0;
}
